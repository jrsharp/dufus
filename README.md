dufus
====

dufus (disk usage for us) is a simple disk usage analysis tool for 9front/Plan 9.

It is inspired by tools like QDirStat, it provides a graphical breakdown of the
on-disk usage of files and directories on a Plan 9 system.  It is meant to
adhere to Plan 9 design principles of simplicity and modularity, doing only the
essential tasks of collecting this file/directory usage information from the
underlying filesystem services and merely providing a basic libdraw visualization
that summarizes this data and allows the user to navigate their filesystem
visually.

It looks like this:

![dufus screenshot](dufus_screenshot.png)

Bugs:

yeah, we've got 'em.
