@echo off
hdc %* forward tcp:50016 tcp:50016
hdc %* shell hilog -b d
hdc %* shell aa start -a EntryAbility -b com.OpenHarmony.app.test
