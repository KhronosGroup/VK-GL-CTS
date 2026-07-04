/*-------------------------------------------------------------------------
 * drawElements Quality Program Platform Utilites
 * ----------------------------------------------
 *
 * Copyright 2015 The Android Open Source Project
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
 * \brief dEQP platform request pixel copy
 *//*--------------------------------------------------------------------*/

package com.drawelements.deqp.platformutil;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.Log;
import android.view.PixelCopy;
import android.view.View;
import android.view.ViewTreeObserver;
import android.view.Window;
import java.io.File;
import java.io.FileOutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class DeqpPlatformRequestPixelCopy {
    private static final String LOG_TAG = "dEQP/PlatformRequestPixelCopy";

    private static class PixelCopyListener
        implements PixelCopy.OnPixelCopyFinishedListener {

        private final Activity activity;
        private final String filename;
        private final Bitmap bmp;
        private final int w;
        private final int h;
        private final HandlerThread ht;

        PixelCopyListener(Activity activity, String filename, Bitmap bmp, int w,
                          int h, HandlerThread ht) {
            this.activity = activity;
            this.filename = filename;
            this.bmp = bmp;
            this.w = w;
            this.h = h;
            this.ht = ht;
        }

        @Override
        public void onPixelCopyFinished(int result) {
            try {
                if (result != PixelCopy.SUCCESS) {
                    Log.e(LOG_TAG, "PixelCopy fail: " + result);
                    return;
                }

                final int[] pixels = new int[w * h];
                bmp.getPixels(pixels, 0, w, 0, 0, w, h);

                final File dir = activity.getExternalFilesDir(null);
                if (dir == null) {
                    Log.e(LOG_TAG, "external files dir is null");
                    return;
                }

                final File tmpFile = new File(dir, filename + ".tmp");
                if (tmpFile.exists())
                    tmpFile.delete();

                FileOutputStream out = new FileOutputStream(tmpFile);
                try {
                    ByteBuffer buffer =
                        ByteBuffer.allocate((pixels.length + 2) * 4);
                    buffer.order(ByteOrder.LITTLE_ENDIAN);

                    buffer.putInt(w);
                    buffer.putInt(h);

                    for (int p : pixels) {
                        buffer.putInt(p);
                    }
                    out.write(buffer.array());

                    out.flush();
                } finally {
                    out.close();
                }

                final File outFile = new File(dir, filename);
                if (outFile.exists()) {
                    outFile.delete();
                }

                if (!tmpFile.renameTo(outFile)) {
                    Log.e(LOG_TAG,
                          "renameTo failed: " + tmpFile.getAbsolutePath() +
                              " -> " + outFile.getAbsolutePath());
                }

            } catch (Exception e) {
                Log.e(LOG_TAG, "PixelCopy write failed", e);
            } finally {
                ht.quitSafely();
            }
        }
    }

    public static void requestPixelCopyToPng(final Activity activity,
                                             final String filename) {
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    final Window window = activity.getWindow();
                    if (window == null) {
                        Log.e(LOG_TAG, "window not found");
                        return;
                    }

                    final View decorView = window.getDecorView();

                    decorView.post(new Runnable() {
                        @Override
                        public void run() {
                            decorView.post(new Runnable() {
                                @Override
                                public void run() {
                                    final int w =
                                        Math.min(decorView.getWidth(),
                                                 decorView.getHeight());
                                    final int h =
                                        Math.max(decorView.getWidth(),
                                                 decorView.getHeight());

                                    if (w == 0 || h == 0) {
                                        Log.e(LOG_TAG,
                                              "decorView size is zero");
                                        return;
                                    }

                                    final Bitmap bmp = Bitmap.createBitmap(
                                        w, h, Bitmap.Config.ARGB_8888);

                                    final HandlerThread ht =
                                        new HandlerThread("PixelCopy");
                                    ht.start();

                                    final Handler handler =
                                        new Handler(ht.getLooper());

                                    final Rect srcRect = new Rect(0, 0, w, h);

                                    PixelCopy.request(
                                        window, srcRect, bmp,
                                        new PixelCopyListener(
                                            activity, filename, bmp, w, h, ht),
                                        handler);
                                }
                            });
                        }
                    });
                } catch (Exception e) {
                    Log.e(LOG_TAG, "Fail", e);
                }
            }
        });
    }
}
