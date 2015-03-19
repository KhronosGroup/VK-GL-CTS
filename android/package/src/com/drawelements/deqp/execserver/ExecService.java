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
 * \brief ExecServer service.
 *//*--------------------------------------------------------------------*/

package com.drawelements.deqp.execserver;

import android.app.Service;
import android.app.Notification;
import android.app.PendingIntent;
import android.content.Intent;
import android.os.Binder;
import android.os.IBinder;

import com.drawelements.deqp.execserver.ExecServerActivity;
import com.drawelements.deqp.R;

public class ExecService extends Service {
	static {
		System.loadLibrary("deqp");
	}

	long m_server = 0; //!< Server pointer.

	// \note No IPC handling, all clients must be in same process
	public class LocalBinder extends Binder {
		ExecService getService () {
			return ExecService.this;
		}
	}

	private final IBinder m_binder = new LocalBinder();

	@Override
	public void onCreate () {
		onCreateNative();
	}

	@Override
	public int onStartCommand (Intent intent, int flags, int startId) {
		// Intent to launch when notification is clicked.
		Intent launchIntent = new Intent(this, ExecServerActivity.class);
		launchIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
		PendingIntent pm = PendingIntent.getActivity(this, 0, launchIntent, 0);

		// Start as foreground service.
		Notification notification = new Notification(R.drawable.deqp_app_small, "dEQP ExecServer", System.currentTimeMillis());
		notification.setLatestEventInfo(this, "dEQP ExecServer", "ExecServer is running in the background.", pm);
		startForeground(1, notification);

		return START_STICKY; // Keep us running until explictly stopped
	}

	@Override
	public IBinder onBind (Intent intent) {
		return m_binder;
	}

	@Override
	public void onDestroy () {
		onDestroyNative();
	}

	private native void onCreateNative	();
	private native void onDestroyNative	();
}
