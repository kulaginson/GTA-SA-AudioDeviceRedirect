# GTA SA Audio Device Redirect

Experimental ASI plugin for GTA San Andreas 1.0 US.

## Purpose

Automatically detect Windows default audio device changes while GTA San
Andreas is running.

Example:

Speakers -> plug in headphones -> Windows changes default device ->
plugin detects it -> GTA process is assigned to the new device.

No GTA restart is required by the plugin itself.

## Target

- GTA San Andreas 1.0 US
- Windows 10 / Windows 11
- Win32 / x86
- ASI Loader

## Build

The project is built automatically using GitHub Actions.

Open:

Actions -> Build GTA SA Audio Device Redirect

After successful build:

Artifacts -> GTA-SA-AudioDeviceRedirect

Download:

AudioDeviceRedirect.asi

## Installation

Install an ASI loader first.

Then put:

AudioDeviceRedirect.asi

next to gta_sa.exe.

Start GTA SA.

## Important

The plugin uses the Windows per-application audio policy interface.

GTA San Andreas uses legacy DirectSound.

On some Windows/GTA configurations, an already-open DirectSound stream may
remain attached to the old endpoint.

If that happens, this project needs the second-stage DirectSound hook that
recreates the GTA audio device after an endpoint change.
