---
name: Bugs
about: Crashes and other bugs
labels: 'bug'

---

### Please fill out the following:
- **Scroll Version:**
  - `scrollmsg -t get_version` or `scroll -v`

- **Debug Log:**
  - Run `scroll -d 2> ~/scroll.log` from a TTY and upload it to a pastebin, such as gist.github.com.
  - This will record information about scroll's activity. Please try to keep the reproduction as brief as possible and exit scroll.
  - Attach the **full** file, do not truncate it.

- **Configuration File:**
  - Please try to produce with the default configuration.
  - If you cannot reproduce with the default configuration, please try to find the minimal configuration to reproduce.
  - Upload the config to a pastebin such as gist.github.com.

- **Stack Trace:**
  - This is only needed if sway crashes.
  - If you use systemd, you should be able to open the coredump of the most recent crash with gdb with
    `coredumpctl gdb scroll` and then `bt full` to obtain the stack trace.
  - If the lines mentioning sway or wlroots have `??` for the location, your binaries were built without debug symbols. Please compile both sway and wlroots from source and try to reproduce.

- **Description:**
  - The steps you took in plain English to reproduce the problem.
