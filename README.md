# MishmeshChild

<p align="center">
  <img src="mishmesh/docs/img/logo.png" alt="mishmesh" width="420">
</p>

<p align="center"><b>A standalone MeshCore pager for kids.</b></p>

<p align="center">
  <a href='https://ko-fi.com/W3V222VPDT' target='_blank'><img height='36' style='border:0px;height:36px;' src='https://storage.ko-fi.com/cdn/kofi6.png?v=6' border='0' alt='Buy Me a Coffee at ko-fi.com' /></a>
</p>

This project combines [MeshCore mishmesh](https://github.com/burakcan/MeshCore-mishmesh)
with ideas and features from
[MeshCore-child-companion](https://github.com/burakcan/MeshCore-child-companion)
to make a child-friendly pager that can still be used as a standalone MeshCore
device. mishmesh provides the on-device messaging interface, contacts, keypad,
and settings; the child-companion inspired additions focus on simple
parent-to-child communication.

### Child pager features

- **Parent controls:** a PIN-protected allowlist controls who can message the
  child. Unknown contacts and public channels are blocked while restrictions
  are enabled.
- **Home:** press Left for Messages or Right for the latest approved chat.
  The chat opens at its newest unread message when one is available.
- **Questions:** send a prompt such as `?Dinner | Pizza | Pasta`; the child
  chooses an answer and it is sent back to you. The alert shows only the prompt
  and opens the answer choices directly.
- **Named timers:** send `%Bitte komme in | 5m`. Accepting shows a large
  countdown, and the alarm shows the original name. The manual Clock timer
  can run at the same time. Chats show the question prompt or timer name.
- **Remote controls:** designate an approved contact to control parental mode,
  GPS, volume, retries, quick replies, the PIN, and the time zone by direct
  message. GPS controls are unavailable to the child while restrictions are active.
- **Call bell:** send one `🔔` to ring the pager and show that Dad is calling.
  The child can reply with `👋 here` from a key press.
- **Delivery:** unsent direct messages retry automatically while the route is
  unavailable.
- **Emoji:** a lightweight monochrome pixel icon set renders the bell and wave
  symbols on the small display.

See [child pager setup and behavior](docs/child-pager.md). Messaging starts
restricted; configure **Apps → Parent controls** before use.

MishmeshChild builds for the Seeed Wio Tracker L1 / L1 Pro with a 128×64 OLED.
The screenshots below show the inherited Mishmesh interface.

### Screens

<table>
  <tr>
    <td align="center"><img src="mishmesh/docs/img/home.png" width="240"><br><sub>Home</sub></td>
    <td align="center"><img src="mishmesh/docs/img/appmenu.png" width="240"><br><sub>App menu</sub></td>
    <td align="center"><img src="mishmesh/docs/img/contacts.png" width="240"><br><sub>Contacts</sub></td>
  </tr>
  <tr>
    <td align="center"><img src="mishmesh/docs/img/contact_detail.png" width="240"><br><sub>Contact detail</sub></td>
    <td align="center"><img src="mishmesh/docs/img/chat.png" width="240"><br><sub>Conversation</sub></td>
    <td align="center"><img src="mishmesh/docs/img/quickreplies.png" width="240"><br><sub>Quick replies</sub></td>
  </tr>
  <tr>
    <td align="center"><img src="mishmesh/docs/img/keypad.png" width="240"><br><sub>Keypad</sub></td>
    <td align="center"><img src="mishmesh/docs/img/repeater.png" width="240"><br><sub>Repeater status</sub></td>
    <td align="center"><img src="mishmesh/docs/img/settings.png" width="240"><br><sub>Settings</sub></td>
  </tr>
  <tr>
    <td align="center"><img src="mishmesh/docs/img/clock_stopwatch.png" width="240"><br><sub>Stopwatch</sub></td>
    <td align="center"><img src="mishmesh/docs/img/clock_timer.png" width="240"><br><sub>Timer</sub></td>
    <td align="center"><img src="mishmesh/docs/img/clock_alarm.png" width="240"><br><sub>Alarm</sub></td>
  </tr>
  <tr>
    <td align="center"><img src="mishmesh/docs/img/clock_world.png" width="240"><br><sub>World clock</sub></td>
    <td></td>
    <td></td>
  </tr>
</table>

### mishmesh features

- **Messaging** - direct messages, channels, and room servers, with delivery
  status, auto-retry, and path reset when a route goes stale.
- **Text entry** - Nokia-style multi-tap keypad with symbols and emoji, in 18
  keyboard layouts. Latin (English, German, French, Spanish, Italian,
  Portuguese, Swedish, Norwegian, Turkish, Polish, Czech, Hungarian) and
  Cyrillic (Russian, Ukrainian, Belarusian, Bulgarian, Serbian, Macedonian).
  Channel keys get a hex-only pad.
- **Contacts** - favourites and per-kind tabs (people, repeaters, rooms),
  rename, ping, telemetry requests, and path management.
- **Repeater management** - log in to a repeater and configure it from the
  device: settings, access list, neighbours, region.
- **Clock** - stopwatch, timer, alarm, and a world clock with DST-aware
  timezones.
- **Airtime** - live duty-cycle and airtime usage against the TX budget.
- **Adverts and sharing** - send adverts and share contacts/channels as QR.
- **Settings on-device** - radio config (frequency, bandwidth, spreading
  factor, TX power), screen sleep, sound, and timezone.
- **First-boot onboarding** - a short wizard for name, region, and time.

### Install

Download the `MishmeshChild-v*_WioTrackerL1_companion_radio_*_mishmesh-child_*.uf2`
for your connection type from [Releases](../../releases), or build it from source.

1. Plug the board into USB.
2. Double-tap reset. It mounts as a USB drive.
3. Drop the `.uf2` onto it. It reboots into MishmeshChild.

The `ble` build pairs with the phone/web app over Bluetooth; the `usb` build
talks over USB serial. Either way the device is fully usable on its own.

Or [build it yourself](#building-from-source).

### Building from source

A PlatformIO project. With the repo cloned:

```sh
pio run -e WioTrackerL1_companion_radio_usb_mishmesh-child -t upload   # or _ble_mishmesh-child
```

Package the `.uf2` / `.zip` like a release does (lands in `out/`):

```sh
sh build.sh build-firmware WioTrackerL1_companion_radio_ble_mishmesh-child
```

### Maintaining the fork

MishmeshChild uses its own version from `mishmesh/ChildVersion.h`. The boot
splash and About screen identify the fork and its Mishmesh base. The firmware
filename includes all three versions and the build commit.

Keep the fork's `upstream` branch at the selected unmodified Mishmesh release.
Fetch new releases from [upstream Mishmesh](https://github.com/burakcan/MeshCore-mishmesh),
fast-forward `upstream` to the chosen tag, and merge that branch into the
fork's `main`. Build the BLE child target before tagging the next MishmeshChild
release as `child-vX.Y.Z`. Attach the packaged firmware from `out/` when
creating the GitHub release and record the exact upstream tag and commit
in its notes. The v0.1.0 base is Mishmesh `mishmesh-v1.7.0` (`1809e674`).

### Emoji

Source builds include a monochrome emoji set based on
[Shuqi Khor’s Pixel Icons](https://github.com/shuqikhor/pixel-icons).
An optional local EmojiMania overlay is described in the
[emoji build guide](docs/mishmesh-emoji-overlay.md).

### How mishmesh fits

The framework lives under [`mishmesh/`](./mishmesh); a thin adapter in
[`examples/companion_radio/ui-mishmesh/`](./examples/companion_radio/ui-mishmesh)
bridges it to the companion app without touching `main.cpp`. Screens are `Applet`
subclasses on a fixed stack managed by `AppletHost`; all drawing goes through
`Canvas`. Text and icons are bitmap fonts rendered with mcufont (Nokia Cellphone
FC, Tom Thumb, hand-edited Cyrillic, Pixelarticons); the logo wordmark is set in the
LastPriestess pixel
font by Christina Antoinette Neofotistou.

Font sources and reproducible generators live in
[`mishmesh/text/fonts`](./mishmesh/text/fonts). Run `build_cyrillic.py` to rebuild
the Cyrillic ranges and `preview_fonts.py` to render review sheets. Open
`font_editor.html` directly from disk to edit `Body.c`, `Subtitle.c`, or
`Caption.c`; the single-file editor does not require a web server.

### AI disclosure

It's mixed. I use AI a lot at work. Side projects are a way for me to keep my
programming skills alive. This project contains a mix of manually written and AI
generated (but manually reviewed) code. I used it more heavily in brainstorming
ideas and discovering the original codebase. Don't use this if you're a purist.

---

## About MeshCore

mishmesh is a fork of **MeshCore**, a lightweight, portable C++ library for
multi-hop packet routing over LoRa and other packet radios. The mesh core, the
companion firmware, supported hardware and the flasher, the phone/web apps, and the
protocol documentation all come from that project. For anything about MeshCore
itself, see the upstream repository:

**https://github.com/meshcore-dev/MeshCore**

## License

MIT, the same as MeshCore.
