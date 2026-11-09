# Stack
![Features](features/Polyphonic-Input--Output-green.svg)
![Features](features/Quantize-Yes-green.svg)<br>
The Stack module is documented alongside the Sample-and-hold modules, as it basically represet a Sample-and-update module which is able to store up to 16 Pushed (sampled) polyphonic values, which it can later pop (retreive). The top section of the module shares a lot with the Sample and Update module, while some of the sections have different names. Below these 3 sections you find a knob to set the size of the stack (1-16). By default it will be set to the maximal size of 16, meaning 16 values (all able to be polyphonic) can be pushed to the stack before it's full. However it do keep track of how many values have been pushed (e.g. you can have a stack with room for up to 16 items, while it might only hold 4 items). The Size knob is how many of the newest stored items you can push, pop, and cycle. Turning it down does not delete the older items; they stay stored and are not shown until you turn the knob back up. Reset clears every stored item, including those hidden ones. *There can be many reasons why you might want a smaller stack size (e.g. it can be used as a rolling stack, where you only want to keep the last `n` pushed items). However unless you have a special reason for a lower stack size, I suggest just leave it at 16.* *Some general info regarding these modules are listed in [Common S&H features](commonShth.md).*

*Further down the Stack Bank Expander (StBk) module is described. Its **Retrieve** copies the entire into the active bank and leaves the Stack module unchanged. Its **Send** first clears all 16 slots in the Stack module, then copies only the slots that bank actually holds, even when that is fewer or more items than the Size knob. The knob is not changed. For example, a bank of 8 items sent while Size is 4 stores all 8, and only the newest 4 can be popped or cycled. Turning Size up to 8 then gives access to the older 4 as well. The Stack output stays at its last Pop or Reset value until the next Pop or Reset.*

The 2-way switch below is used to set the mode of the stack: **FIFO** (First-In-First-Out) or **LIFO** (Last-In-First-Out). Changing this setting does not rearrange the items already stored. Push always adds the new item as the newest. The switch only chooses which end **Pop** takes from: in **FIFO** the oldest item is popped, and in **LIFO** the newest item is popped.

At the bottom of the module you find the polyphonic input (which is sampled on Push), and output (which is updated on Pop and Reset). Quantize is set from the context menu and is off by default. Push stores the raw input. Quantize and clip are applied when a value is written to the Stack output. The Stack expander has its own Quantize menu for the Value output. *Saving and loading a patch, the full internal stack of the Stack module is saved/loaded along with the patch.*

![Screenshot of Stack](module/Stack.png)

In each of the 3 top sections you find a momentary button (for manual operation) and a trigger-input. Whenever the **Push-button** is pressed (or a Push-trigger is received) the input is sampled and pushed to the stack. Next to the "Push" label you find a small light, which will change it's color based on the status of the stack:
+ Fully green when the stack is empty.
+ Gradually transitioning to amper as the stack begins to fill (less "free space" in the stack).
+ Fully red when the stack is full (according to selected stack-size).

Between the Push button and the tigger-input you find a small mode toggle-button. This button toggles the Push-mode which basically determines how the modules behaves when a **Push request is received while the stack is currently full** - based on the stack-size set by the Stack knob (1-16 slots in the stack):
+ **Dim/Ignore**: Push is ignored when the stack is full.
+ **Red/Oldest** (rolling): The oldest accessible element is dropped and the new sample is pushed as newest, so the accessible window stays at the stack size. Rolling does not change direction with LIFO/FIFO — those modes only affect which end **Pop** takes from.

Likewise in the Pop section you also find a momentary button (for manual operation), a trigger-input, and a mode toggle-button between them. Whenever the **Pop-button** is pressed (or a Pop-trigger is received) an item is **popped from the stack** and the output is updated. Which item is popped, is based on the 2-way stack mode switch. When the 2-way switch is in **FIFO** mode the **first/oldest** element element is popped.  When in **LIFO** mode, the **last/newest** element added to the stack is popped. Next to the "Pop" label you find a small light, which will change it's color based on the status of the stack:
+ Fully green when the stack is full.
+ Gradually transitioning to amper as the stack begins to empty (fewer elements to pop).
+ Fully red when the stack is empty (nothing to pop).

The mode toggle-button between the momentary button and the trigger-input determines what is going to occur when **Pop request is received while the stack is empty**:
+ **Dim/Zero**: Update output to 0V.
+ **Red/Hold**: Hold current value(s).

In the reset setion you also find a momentary button (for manual operation), a trigger-input and a mode toggle-button between them. Whenever the **Reset-button** is pressed (or a Reset-trigger is received) the stack will be reset (emptied). The mode toggle-button between the momentary button and the trigger-input more or less works in the same manner as the Pop-modes:
+ **Dim/Zero**: Stack is cleared, and update output to 0V.
+ **Red/Hold**: Stack is cleared, but output will hold current value(s).

*Initializing the module will always clear the stack, and update all outputs to 0V*

Both the input- and output-ports at the bottom of the module supports **Polyphonic signals**. The polyphonic output will be determined primarily by the input, and secondary by the stack (when popping items). If a cable is connected to the input-port, the output port will always output as many channels as the input. However when no input-cable is connected the output channel-count will be based on the content of the stack. This allows you to connect an input cable, push one or more items to the stack. If you later disconnect this cable, and starts popping items, it will output as many channels as was pushed to the stack while the input was connected. You can, but **SHOULD NOT** push items with different channel counts, as modules downstream might not "behave nicely" when its input keeps changing channel count. If/when you insert a new cable with a different channel count than what you might have used previously, it's a **good idea to press the "Reset" button** to ensure the stack is empty.

*If the items in the stack have a lower channel count (e.g. 4) than the currently connected input (e.g. 6) the last channels of the output (2) will simply be set to 0V. On the other hand if the items in the stack have a higher channel count than a connected input, the additional channels will be ignored (the module will then only output as many channels as the connected input).*

While the input- and output ports are polyphonic the **Push-, Pop- and Reset-triggers are monophonic**, so each Push-, Pop- and Reset operation will always affect all (up to) 16 channels. E.g. with an 8-channel polyphonic input, all 8 channel-values will be pushed to/popped from the stack. Each of the (up to) 16 stack elements can hold 16 values (one for each channel). *Hence the internal stack have room for 256 values (16x16)*

Other S&H modules: [S&H/T&H-2](SHTH2.md), [S&H/T&H-2x4](SHTH2x4.md), [Sample and Update](SampleAndUpdate.md), [Stack expander](StackExpander.md), [Stack bank expander](StackBankExpander.md).

[Go back to modules overview](plugin.md#modules)
