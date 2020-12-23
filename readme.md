# pcdmon

Render a visualization for an individual Windows performance counter.

<div align="center">
<img src="https://i.imgur.com/j36eTpa.png">
</div>

**Usage:**

Make sure you've got the [VC++ Runtime](https://support.microsoft.com/en-us/help/2977003/the-latest-supported-visual-c-downloads) installed.

`pcdmon {Counter Path} [Polling Interval]`

where `{Counter Path}` is a valid Windows [Counter Path](https://docs.microsoft.com/en-us/windows/win32/perfctrs/specifying-a-counter-path)

A noteworthy shortcut to enumerate existing paths on a given machine is to simply use PowerShell:

`> Get-Counter -ListSet "{Counter Object}" | Get-Counter`

where {Counter Object} is a valid Windows [counter object](https://docs.microsoft.com/en-us/previous-versions/windows/it-pro/windows-server-2003/cc783073(v=ws.10))

Then supply pcdmon with a single argument containing the desired counter path, for example:

`pcdmon "\network interface(intel[r] wi-fi 6 ax200 160mhz)\bytes received/sec"`

*Remember to wrap the argument in quotes if it contains spaces*

The default polling interval is 1 second (1000ms), which is adjustable with the keybindings listed below. The minimum and maximum permitted polling intervals are 100ms and 10000ms respectively. Numerous performance counters are implemented as rate-counters which require more than a single sample to produce a meaningful result, so use caution when passing an interval argument. 

---

**More path examples**

Multiple instances of objects that share the same name string can be indexed with a pound sign

`\Process(chrome)\IO Read Bytes/sec`

`\Process(chrome#1)\IO Read Bytes/sec`

`\Processor(_Total)\% Processor Time`

`\System\File Write Operations/sec`

---

**Key bindings**

`<Escape>` - Quit

`<F1>` - Enable window decorations

`<F2>` - Disable window decorations

`<Arrow Up>` - Increase polling interval by 100ms

`<Arrow Down>` - Decrease polling interval by 100ms

`<W>` - Shift projector position up

`<A>` - Shift projector position left

`<S>` - Shift projector position down

`<D>` - Shift projector position right

`<R>` - Reset projector position

---

Further reading:

[performance counters](https://docs.microsoft.com/en-us/windows/win32/perfctrs/using-performance-counters)

[win32 API](https://docs.microsoft.com/en-us/windows/win32/)

[libsdl](https://www.libsdl.org/)
