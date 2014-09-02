/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief dEQP instrumentation
 *//*--------------------------------------------------------------------*/

package com.drawelements.deqp.testercore;

import android.app.ActivityManager;
import android.app.Instrumentation;
import android.app.Activity;
import android.app.NativeActivity;

import android.content.Context;
import android.content.Intent;
import android.content.ComponentName;

import android.os.Bundle;

import java.util.List;
import java.lang.Thread;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;

public class DeqpInstrumentation extends Instrumentation
{
	private static final String	LOG_TAG		= "dEQP/Instrumentation";
	private static final long	TIMEOUT_MS	= 40000; // 40s Timeouts if no log is produced.

	private String				m_cmdLine;
	private String				m_logFileName;
	private boolean				m_logData;

	@Override
	public void onCreate (Bundle arguments) {
		super.onCreate(arguments);
		start();

		m_cmdLine		= arguments.getString("deqpCmdLine");
		m_logFileName	= arguments.getString("deqpLogFilename");

		if (m_cmdLine == null)
			m_cmdLine = "";

		if (m_logFileName == null)
			m_logFileName = "/sdcard/TestLog.qpa";

		if (arguments.getString("deqpLogData") != null)
		{
			if (arguments.getString("deqpLogData").compareToIgnoreCase("true") == 0)
				m_logData = true;
			else
				m_logData = false;
		}
		else
			m_logData = false;
	}

	@Override
	public void onStart () {
		super.onStart();

		try
		{
			Log.d(LOG_TAG, "onStart");

			String			testerName		= "";
			RemoteAPI		remoteApi		= new RemoteAPI(getTargetContext(), m_logFileName);
			TestLogParser	parser			= null;
			long			lastMoreLogMs	= 0;

			{
				File log = new File(m_logFileName);
				if (log.exists())
					log.delete();
			}

			remoteApi.start(testerName, m_cmdLine, null);

			// Wait for execution to start
			Thread.sleep(1000); // 1s

			parser = new TestLogParser();
			parser.init(this, m_logFileName, m_logData);

			lastMoreLogMs = System.currentTimeMillis();

			while (remoteApi.isRunning())
			{
				if (parser.parse())
					lastMoreLogMs = System.currentTimeMillis();
				else
				{
					long currentTimeMs = System.currentTimeMillis();

					if (currentTimeMs - lastMoreLogMs >= TIMEOUT_MS)
					{
						remoteApi.kill();
						break;
					}

					Thread.sleep(100); // Wait 100ms
				}
			}

			parser.parse();
			parser.deinit();

			finish(0, new Bundle());
		}
		catch (Exception e)
		{
			Bundle info = new Bundle();

			info.putString("Exception", e.getMessage());
			finish(1, new Bundle());
		}
	}

	public void testCaseResult (String code, String details)
	{
		Bundle info = new Bundle();

		info.putString("dEQP-EventType", "TestCaseResult");
		info.putString("dEQP-TestCaseResult-Code", code);
		info.putString("dEQP-TestCaseResult-Details", details);

		sendStatus(0, info);
	}

	public void beginTestCase (String testCase)
	{
		Bundle info = new Bundle();

		info.putString("dEQP-EventType", "BeginTestCase");
		info.putString("dEQP-BeginTestCase-TestCasePath", testCase);

		sendStatus(0, info);
	}

	public void endTestCase ()
	{
		Bundle info = new Bundle();

		info.putString("dEQP-EventType", "EndTestCase");
		sendStatus(0, info);
	}

	public void testLogData (String log) throws InterruptedException
	{
		if (m_logData)
		{
			final int chunkSize = 4*1024;

			while (log != null)
			{
				String message;

				if (log.length() > chunkSize)
				{
					message = log.substring(0, chunkSize);
					log = log.substring(chunkSize);
				}
				else
				{
					message = log;
					log = null;
				}

				Bundle info = new Bundle();

				info.putString("dEQP-EventType", "TestLogData");
				info.putString("dEQP-TestLogData-Log", message);
				sendStatus(0, info);

				if (log != null)
				{
					Thread.sleep(1); // 1ms
				}
			}
		}
	}

	public void beginSession ()
	{
		Bundle info = new Bundle();

		info.putString("dEQP-EventType", "BeginSession");
		sendStatus(0, info);
	}

	public void endSession ()
	{
		Bundle info = new Bundle();

		info.putString("dEQP-EventType", "EndSession");
		sendStatus(0, info);
	}

	public void sessionInfo (String name, String value)
	{
		Bundle info = new Bundle();

		info.putString("dEQP-EventType", "SessionInfo");
		info.putString("dEQP-SessionInfo-Name", name);
		info.putString("dEQP-SessionInfo-Value", value);

		sendStatus(0, info);
	}

	public void terminateTestCase (String reason)
	{
		Bundle info = new Bundle();

		info.putString("dEQP-EventType", "TerminateTestCase");
		info.putString("dEQP-TerminateTestCase-Reason", reason);

		sendStatus(0, info);
	}

	@Override
	public void onDestroy() {
		Log.e(LOG_TAG, "onDestroy");
		super.onDestroy();
		Log.e(LOG_TAG, "onDestroy");
	}
}
