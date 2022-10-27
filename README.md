KERNEL OVERLAY HIDER (Direct Kernel Object Manipulation)

https://streamable.com/dfy6d9

Tested on: BattlEye over the period of 3 months

This project was mainly made out of curiosity towards how Windows handles the windows in kernel.
I also wanted to learn how to hide the window from possible enumerations and anti cheats.

This project gets a pointer to an undocumented structure TAG_WND, then modifies the members to make your window invisible from possible HWND scans.
The structures hold two particularly interesting members, Next and Previous. These can be thought as Flink and Blink.

My driver modifies the Flink and Blink so that our desired window handle will never be in the list, making it invisible to enumerations and window callbacks.

CAUTION:
This project manipulates kernel objects and undocumented structures which can cause blue screen of death and PC damages if you don't know what you are doing.
As an author of this project, I take no liability of damages caused to your computer because of this project.


RUNNING THIS PROJECT UNMODIFIED WILL CAUSE A BLUE SCREEN OF DEATH INEVITABLY BECAUSE I DO NOT RESTORE THE WINDOW, I LEAVE IT UP TO THE READER TO SOLVE SO ITS NOT EASY TO COPY+PASTE.

I do not want this project to be a 1:1 copy paste for possible game cheaters, this is a learning resource. If you want this to be reliable, you need to fix it yourself like I have done.

That's it for today, hope you like this release! Make sure to leave a star if you found this helpful.
