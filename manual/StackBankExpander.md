# Stack bank expander
![Features](features/Expander--for-Stack-green.svg)
![Features](features/Polyphonic-No-red.svg)<br>
As the name implies, this module is bank-expander (StBk) for the Stack module. Internally it have 4 banks, where each of these banks can contain a full stack, with a stack-size of up to 16 (polyphonic) elements. Saving and loading a patch persists the content of all 4 banks, along with which bank is active. The Stack bank expander can be used to store- and switch between (banked) stacks, where each stack can be regarded as a sequence of CV-values (e.g. V/OCT notes). E.g. you can Push (up to 16) polyphonic elements into the the internal stack of the Stack module. Then you can retreive this entire stack, and store it into the active selected bank of the StBk. As the StBk have 4 banks, you are able to store 4 full stacks. Afterwards you can switch between these banks, and send the active bank back into the internal stack of the Stack module (e.g. to switch between previously "stored" sequence of notes). *Each Pop performed by the Stack module, will pop (remove) an item from its stack, however it will not affect the stored banks of the StBk. Hence the same/full stack can be send multiple times from the StBk to the Stack module.* *Some general info regarding these modules are listed in [Common S&H features](commonShth.md).*

The 4 momentary buttons at the top can be used to select the active bank (indicated by the lit light above each button). Switching bank in the StBk will have no effect on the Stack module itself, as it always have- and uses it's internal stack. **Switching bank using these 4 buttons will not send the selected bank to the stack module**.

Below you find the cycle section, where you in the top have a a momentary button and trigger-input to trigger the cycle action. Between these two there is a small button which toggles the cycle mode:

+ **Green/Next**: Each cycle will switch to the next bank.
+ **Red/Prev**: Each cycle will switch to the previous bank.
+ **Yellow/Ping-pong**: First it will switch in one direction till the end is reached, then it will switch in the other directon.
+ **Orange/Random**: Each cycle will switch to a random bank (perhaps the same as the current).
+ **Blue/Random diff.**: Each cycle will switch to a different random bank (never the same as current).

At the bottom of the cycle section there is a **3-way switch**. Down (the default) cycles banks 1 and 2, the middle position cycles banks 1–3, and up cycles banks 1–4. The four buttons at the top can still select any bank. This switch only affects the **Cycle button/trigger**. Whenever the cycle button is pressed or the cycle-trigger is activated, the module switches among the banks included by the switch. E.g. with the switch down, it switches between bank 1 and bank 2.

By default a **Cycle will only select an active bank**, however it will not Send the newly selected bank to the Stack module. However using the context menu you can select that a cycle should **automatic send the newly selected bank to the Stack module**. When enabled, the small red light next to the Cycle label will illuminate. The reset button and its trigger-input will simply select bank 1 (basically it have the same effect as pressing the cycle button, until bank 1 is the active). Whether or not bank 1 will be automatic send to the Stack module depends on the previously menioned context menu.

Below you find the Send and Retrieve sections (each with a momentary button and a trigger input). Select the desired bank using the 1-4 buttons in the top of the module, then **Retrieve** to copy the Stack module's accessible items into that bank (the Stack module is left unchanged), or **Send** to clear all 16 slots of the Stack module and then copy only the slots that bank actually holds. Send does not change the Size knob. Items beyond the current Size stay stored until the knob is raised or the stack is reset. Send does not change the Stack output; it stays at its last Pop or Reset value. Likewise if a Stack expander is also connected with the Stack, its Value output in the cycle-section will not change when you send a bank.

![Screenshot of Stack bank expander](module/StackBankExpander.png)

Other S&H modules: [S&H/T&H-2](SHTH2.md), [S&H/T&H-2x4](SHTH2x4.md), [Sample and Update](SampleAndUpdate.md), [Stack](Stack.md), [Stack expander](StackExpander.md).

[Go back to modules overview](plugin.md#modules)
