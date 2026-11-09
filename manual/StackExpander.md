# Stack expander
![Features](features/Expander--for-Stack-green.svg)
![Features](features/Polyphonic-Output-orange.svg)
![Features](features/Quantize-Yes-green.svg)<br>
The Stack expander will (when placed next to a Stack module) expand the Stack module with additional features. At the top you find 16 lights in a 4×4 grid. The newest item is the top-left light, and older items follow left to right, then top to bottom. If the stack size is 8, the top two rows are used and the lights past Size stay off. Filled slots are **green** and empty slots inside the Size are **red**. For example, with Size 8 and only 2 items pushed, 2 lights are green and the other 6 in those two rows are red. *Some general info regarding these modules are listed in [Common S&H features](commonShth.md).*

Below you find the **Cycle section** which enables you to loop through the items currently in the stack without popping (removing) them. At the top of this section you find the manual cycle button and its associated trigger-input. When the cycle button is pressed (or a cycle-trigger is received) the module steps to another item and copies it to the **Value** output. This enables you to push multiple values (e.g. 8 or 16 values forming a sequence of V/OCT notes) to the stack, and then while leaving this sequence in the stack, keep looping it multiple times. Via the context menu you can choose the direction of the cycle, which is indicated by the color of the light next to the "Cycle" label:
+ **Green/Next**: Walks from the oldest item toward the newest, then wraps.
+ **Red/Prev**: Walks the other way, from the newest item toward the oldest, then wraps.
+ **Yellow/Ping-pong**: Plays each end once, then reverses.
+ **Orange/Random**: A random item, which may be the current one.
+ **Blue/Random diff.**: A different random item. With only one item in the stack, it stays on that item.

Between the cycle button and the cycle trigger-input you find a button to toggle the cycle-mode. Basically this button determins what the module will do then you perform the cycle action while the stack is empty:
+ **Dim/Zero**: Update output to 0V.
+ **Red/Hold**: Hold current value(s).

Below the cycle-trigger you find the small **Reset button** and **Reset trigger**. When you press the small reset button (or input a reset-trigger), the module copies the **oldest** item in the stack to the Value output. If the stack is empty, the output follows the cycle-mode mentioned above, where the output will either be set to 0V or hold current value(s).

The Value output is a **frozen copy** of that slot, including its channel count. That held element (up to 16 channel values) is saved and loaded with the patch. Quantize for Value is set from this expander's context menu and is off by default. The saved copy stays raw, and the selected mode is applied when Value is written, including when you change the mode. The blue light is not restored until the next Cycle or Cycle Reset, because the stack itself is matched to that slot only while the module is running. Push, Pop, Reset, shrinking Size, and Send do not change it. Within the 16 lights, the light for the slot last copied by Cycle or Cycle Reset is shown in **blue** instead of green, and only while that slot is inside the Size window. If it is popped, dropped by a rolling push, cleared by Reset, replaced by Send, or only hidden because Size was turned down, the light is no longer blue. If a hidden slot becomes accessible again before the next Cycle, the blue light can return. The Value port keeps the last copy. The next Cycle then starts over: Next and ping-pong begin at the oldest item, and Prev begins at the newest.

At the bottom of the module you find 4 outputs, where the first 2 are trigger outputs, and the last are gate outputs:
+ **OvFlw** (overflow): Outputs a trigger when the stack is full (acording to the selected stack-size), and it have recieved a Push-request.
+ **UnFlw** (underlow): Outputs a trigger when the stack is empty, and it have recieved a Pop-request.
+ **Full**: Outputs a high gate when the stack is full (acording to the selected stack-size).
+ **Empty**: Outputs a high gate when the stack if empty.

![Screenshot of Stack Expander](module/StackExpander.png)

**TIP**: If you you feed the Empty gate into the Cycle- and Send inputs of the [Stack bank expander](StackBankExpander.md), when you pop the last element from the stack, a previously banked stack will be selected- and send from the Stack bank expander, and the Stack module will have received a new stack to pop from. Doing so, the Empty gate will typical only be high for a few cycles, as the stack is quickly repopulated once it is empty. If you need to, you can run the empty gate through one of the [Slew modules](Slew2.md) (with no rise-time, but a "longer" fall-time and perhaps an exponential fall shape), to have a signal that will be detected as a high gate for a longer time. *For an expander to work it must be placed left/right of the module it should expand. Hence in this configuration the Stack module needs to be plance between a Stack expander and a Stack bank expander (no matter which is on the left/right side of the Stack module).*

Other S&H modules: [S&H/T&H-2](SHTH2.md), [S&H/T&H-2x4](SHTH2x4.md), [Sample and Update](SampleAndUpdate.md), [Stack](Stack.md), [Stack bank expander](StackBankExpander.md).

[Go back to modules overview](plugin.md#modules)
