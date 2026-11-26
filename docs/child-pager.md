# Child pager

The mishmesh companion firmware provides PIN-protected parent controls, question
buttons, call bells, and automatic delivery retries. The message formats follow
[MeshCore-child-companion](https://github.com/burakcan/MeshCore-child-companion).

The firmware bundles 45 monochrome 9×9 emoji glyphs based on
[Pixel Icons](../mishmesh/text/pixel-icons/README.md), including the bell and a
waving-hand adaptation. They appear in messages and the emoji picker. This is a
selected set; unsupported emojis still show a placeholder.

## Parent setup

1. Finish the device's normal radio setup. Use the same radio settings as the
   parent's MeshCore device.
2. Open **Apps → Parent controls** and choose a six-digit PIN. Up/down changes
   a digit; press selects the next digit and submits the sixth. Enter it again
   to confirm. Left/right moves between digits.
3. Keep **Restricted messaging** enabled. Select **Add allowed contact** and
   select the parent's chat contact. Existing contacts and recently heard adverts
   are available here; send an advert from the parent's device if it is missing.
   The key suffix next to the name helps distinguish devices with the same name.
4. Add any other trusted chat contacts, then select **Lock parent controls** or
   leave the screen. Send a test direct message in both directions.

Up to 32 full public keys can be approved. Adding, importing, renaming or marking
a contact as a favourite does not approve it. The parent menu also removes
approved contacts, changes the PIN, and can disable restricted messaging.
Unlocking the menu alone never disables message filtering.

The menu locks when it closes, sleeps, is covered by another screen, or after two
minutes. Incorrect PINs incur a 30-second wait. There is also a 30-second wait
after reboot, preventing rebooting from skipping the delay. The PIN is stored as
a salted hash. Changes are saved before they take effect; corrupt policy storage
blocks communication.

Fresh devices start with no approved contacts and block messages until setup.
While restricted, only approved chat contacts can send or receive direct
messages. Channels and room posts are blocked, including saved channel history.
No Public channel is seeded. Unknown adverts are kept only in the temporary
discovery list for the unlocked parent menu; they do not auto-add contacts,
appear in the child's discovery list, or trigger a notification. Companion-app
configuration commands cannot bypass these controls. Previously saved hidden
conversations remain on flash, so disabling the restriction can reveal them.

The PIN controls this firmware's settings; it is not secure boot or protection
against physically erasing/reflashing the device. After an erase, a parent must
set up the PIN and approved contacts again.

## Questions

Send an ordinary direct message from an approved contact:

```text
?Dinner | Pizza | Pasta
```

Opening the question shows its choices. Up/down selects an answer; press sends
the selected text, for example `Pasta`, back to the sender. Selecting a question
in older history also opens its choices. Answer state is saved, so reopening an
answered question does not send it twice. If sending cannot be queued, the child
can try again; a reply marked failed after reboot can also be retried.

Use two to six nonempty choices within MeshCore's 160-byte message limit. Spaces
around the prompt and answers are trimmed; UTF-8 text is preserved. Malformed
questions remain ordinary messages. Only direct messages are interpreted.

## Remote lock and unlock

Enter the PIN in **Apps → Parent controls**. Highlight an approved contact
and press Right to designate it as the remote parent. The entry starts with
**Remote**. Press Right on that entry again to turn remote control off.

From that contact, send `!parent lock` to enable restricted messaging or
`!parent unlock` to disable it. Use a direct message. These commands change
the mode without opening the PIN-protected settings.

The same remote parent can send these direct messages:

| Command | Effect |
| --- | --- |
| `!pin <old> <new>` | Changes the six-digit parent PIN. The sending app may retain both PINs in its sent history. |
| `!tz <minutes>` | Sets a fixed UTC offset in 15-minute increments, from −720 to +840. |
| `!preset <n> <text>` | Updates quick reply n or appends at the next available slot, up to 10. Omit the text to remove that entry and shift later entries up. |
| `!name <name>` | Changes the remote parent's local contact name on the pager. |
| `!retry on` / `!retry off` | Controls automatic direct message retries. |
| `!gps on` / `!gps off` | Controls the GPS receiver. The local GPS toggle is unavailable while restrictions are active. |
| `!sound low\|mid\|high\|mute` | Sets system volume. The child can also adjust volume locally. |

The pager handles these commands silently.

## Timer requests

Send `%Bitte komme in | 5m` as a direct message. The pager shows the prompt
above a large `5:00`. Press Select to start the countdown or Back to dismiss
the request. When the countdown ends, the pager opens its timer alarm, even
if the display is asleep.

Use a whole number followed by `s`, `m`, or `h` (up to eight hours).
The text before the separator is the timer name. After Select, the pager
keeps the name and large countdown on screen. Back hides the countdown
while it runs, so you can start a separate manual timer in **Clock**.
When the message timer finishes, the pager shows its name with **Time's up!**.
Additional message requests wait until the current message timer is dismissed.

## Call bell

Send a single `🔔` as a direct message. The display wakes, rings, and shows
**Dad is calling** when the sender is named Dad (other callers use their own
contact names). Any key queues `👋 here` back to that caller. A failed enqueue
keeps the alert open for another press.

The bell repeats about every 3.5 seconds for up to 45 seconds. After that it
stops ringing but keeps the prompt; timing out never sends a human reply. Sound
uses the alarm tone at medium volume, subject to the sound engine's enable and
category settings. Up to four calls can wait in order; additional bells remain
ordinary messages in history. Each queued call keeps its own reply target.

## Delivery

On-device direct messages, including question answers and bell replies, retry
until acknowledged while the device remains running. Delays start with the
estimated route timeout, double, and cap at three minutes. Heard radio traffic
can bring a retry forward, with at least three seconds between attempts. Hearing
the recipient's advert schedules a retry with a fresh flood route. Route recovery
also runs every second retry. Per-chat regions apply to retransmissions.

The outbox holds eight pending direct messages. When full, sending reports
failure instead of silently dropping an older pending message. Received retries
with the same sender, timestamp and body are acknowledged without another stored
message or alert, as long as the original remains in history. Removing permission
also blocks further sends and retries to that contact.

Retry state is RAM-only, matching the reference firmware: rebooting marks old
pending messages failed; it does not silently resend them. The child can retry
failed messages explicitly. **Delivered** means a radio acknowledgement was
received; `Pasta` or `👋 here` is the separate response from the child.

## Validation

Native tests cover PIN/session persistence, failed storage, full-key matching,
channel blocking, question selection, bell targeting and timeout, duplicate
recognition, outbox limits and retry timing. The Wio Tracker L1 USB build target
is `WioTrackerL1_companion_radio_usb_mishmesh`. Physical display, buzzer and
two-device radio testing are still required before handing over the pager.
