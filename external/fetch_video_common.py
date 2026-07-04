# -*- coding: utf-8 -*-

# -------------------------------------------------------------------------
# fetch_video_common.py
# --------------------------------------
#
# Copyright 2015 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# -------------------------------------------------------------------------

"""Shared download mechanics for the video sample fetch scripts.

Each fetch_video_*_samples.py script defines its own list of assets and
calls main() with it; all the download/verify/clean logic lives here.
"""

import argparse
import hashlib
import os
import ssl
import sys
from urllib.request import urlopen

SCRIPT_PATH = os.path.join(os.path.dirname(__file__), "..", "scripts")
sys.path.insert(0, SCRIPT_PATH)

from ctsbuild.common import initializeLogger  # noqa: E402

EXTERNAL_DIR = os.path.realpath(os.path.normpath(os.path.dirname(__file__)))
GCS_BASE_URL = "https://storage.googleapis.com/vulkan-video-samples"
VIDEO_DEST_DIR = "vulkancts/data/vulkan/video"


CHUNK_SIZE = 1024 * 1024


def checksum_file(path):
    """Return the SHA-256 hex digest of a file, read in chunks."""
    digest = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(CHUNK_SIZE), b""):
            digest.update(chunk)
    return digest.hexdigest()


def format_size(num_bytes):
    """Return a human-readable size such as '4.0 MiB'."""
    size = float(num_bytes)
    unit = "B"
    for unit in ("B", "KiB", "MiB", "GiB", "TiB"):
        if size < 1024.0:
            break
        size /= 1024.0
    return f"{size:.1f} {unit}"


class ProgressBar:
    """A minimal carriage-return progress bar for a single download.

    Animates only when stderr is a TTY; otherwise stays silent so piped
    output and CI logs are not flooded with carriage returns. When the
    total size is unknown (no Content-Length) it reports bytes received
    without a percentage.
    """

    WIDTH = 30

    def __init__(self, total):
        self.total = total
        self.done = 0
        self.enabled = sys.stderr.isatty()

    def update(self, n):
        """Record n more bytes received and redraw the bar."""
        self.done += n
        if not self.enabled:
            return
        if self.total:
            frac = min(self.done / self.total, 1.0)
            filled = int(self.WIDTH * frac)
            fill = "#" * filled + "-" * (self.WIDTH - filled)
            sys.stderr.write(
                f"\r  [{fill}] {frac * 100:5.1f}% "
                f"({format_size(self.done)} / {format_size(self.total)})"
            )
        else:
            sys.stderr.write(f"\r  {format_size(self.done)} downloaded")
        sys.stderr.flush()

    def finish(self):
        """Terminate the bar's line, at most once."""
        if self.enabled:
            self.enabled = False
            sys.stderr.write("\n")
            sys.stderr.flush()


class SourceFile:
    """A single downloadable media sample with a known SHA-256 checksum.

    url_path is appended to GCS_BASE_URL to form the download URL.
    dest is the path under VIDEO_DEST_DIR where the file is stored; it
    defaults to url_path, which is the layout used by every sample.
    """

    def __init__(self, url_path, checksum, dest=None):
        self.url = f"{GCS_BASE_URL}/{url_path}"
        self.checksum = checksum
        self.dest = dest if dest is not None else url_path

    def full_path(self):
        """Return the absolute destination path for this sample."""
        return os.path.join(EXTERNAL_DIR, VIDEO_DEST_DIR, self.dest)

    def clean(self):
        """Remove the local copy of this sample, if present."""
        try:
            os.remove(self.full_path())
        except OSError:
            pass

    def update(self, force=False, insecure=False):
        """Download the sample if missing, mismatched, or force is set.

        Return "downloaded" if the file was fetched, "checked" if the
        existing local copy already matched the expected checksum.
        """
        if force or not self.is_up_to_date():
            self.clean()
            self.fetch_and_verify(insecure)
            return "downloaded"
        return "checked"

    def is_up_to_date(self):
        """Return True if the local file exists and matches the checksum."""
        path = self.full_path()
        if not os.path.exists(path):
            return False
        return checksum_file(path) == self.checksum

    def fetch_and_verify(self, insecure):
        """Stream the sample to disk and verify its SHA-256 checksum.

        The download is written to a temporary file alongside the final
        destination so the multi-gigabyte samples never need to fit in
        RAM. The temporary file is renamed into place only once the
        checksum matches, so an interrupted or corrupt download never
        leaves a bad file at the real path.
        """
        print(f"Fetching {self.url}")

        if insecure:
            print("Ignoring certificate checks")
            ctx = ssl._create_unverified_context()
            req = urlopen(self.url, context=ctx)
        else:
            req = urlopen(self.url)

        dst_path = self.full_path()
        os.makedirs(os.path.dirname(dst_path), exist_ok=True)

        total = int(req.headers.get("Content-Length") or 0)
        progress = ProgressBar(total)

        digest = hashlib.sha256()
        tmp_path = dst_path + ".part"
        try:
            with req, open(tmp_path, "wb") as out:
                for chunk in iter(lambda: req.read(CHUNK_SIZE), b""):
                    digest.update(chunk)
                    out.write(chunk)
                    progress.update(len(chunk))

            checksum = digest.hexdigest()
            if checksum != self.checksum:
                raise RuntimeError(
                    f"Checksum mismatch for {self.dest}, "
                    f"expected {self.checksum}, got {checksum}"
                )

            os.replace(tmp_path, dst_path)
        except BaseException:
            # Leave no partial file behind on failure or interruption.
            try:
                os.remove(tmp_path)
            except OSError:
                pass
            raise
        finally:
            progress.finish()

        print(f"Downloaded and verified: {dst_path}")


def parse_args(description):
    """Parse the common fetch command-line arguments."""
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument('--clean', action='store_true', default=False,
                        help='Remove sample files instead of fetching')
    parser.add_argument('--force', action='store_true', default=False,
                        help='Re-download even if checksums match')
    parser.add_argument('--insecure', action='store_true', default=False,
                        help='Disable TLS certificate checks')
    parser.add_argument('-v', '--verbose', action='store_true',
                        help='Enable verbose logging')
    return parser.parse_args()


def main(packages, description="Fetch video sample data"):
    """Run the fetch/clean loop over packages, driven by CLI arguments."""
    args = parse_args(description)
    initializeLogger(args.verbose)

    downloaded = 0
    checked = 0
    try:
        for pkg in packages:
            if args.clean:
                pkg.clean()
                continue
            result = pkg.update(force=args.force, insecure=args.insecure)
            if result == "downloaded":
                downloaded += 1
            else:
                checked += 1
    except KeyboardInterrupt:
        sys.exit("")

    if not args.clean:
        print(f"Done: {downloaded} downloaded and verified, "
              f"{checked} already present and verified "
              f"({len(packages)} total)")
