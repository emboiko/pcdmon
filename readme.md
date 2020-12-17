# pcdmon

Render a visualization for an individual Windows performance counter.

<div align="center">
<img src="https://i.imgur.com/JhmFtLn.png">
</div>

**Usage:**

`pcdmon {Counter Path}`

where `{Counter Path}` is a valid Windows [Counter Path](https://docs.microsoft.com/en-us/windows/win32/perfctrs/specifying-a-counter-path). 

A noteworthy shortcut to enumerate existing paths on a given machine is to simply use PowerShell:

`> Get-Counter -ListSet "{Counter Object}" | Get-Counter`

where {Counter Object} is a valid Windows [counter object](https://docs.microsoft.com/en-us/previous-versions/windows/it-pro/windows-server-2003/cc783073(v=ws.10))

Then supply pcdmon with a single argument containing the desired counter path, for example:

`pcdmon "\network interface(intel[r] wi-fi 6 ax200 160mhz)\bytes received/sec"`

Remember to wrap the argument in quotes if it contains spaces


---

`<Escape>` - Quit

`<F1>` - Enable window decorations

`<F2>` - Disable window decorations

---

Further reading:

[performance counters](https://docs.microsoft.com/en-us/windows/win32/perfctrs/using-performance-counters)



[win32 API](https://docs.microsoft.com/en-us/windows/win32/)

[libsdl](https://www.libsdl.org/)

---

Todo:

- Viewports
- Render text (?)
- Accept multiple counters