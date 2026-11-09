# Sample and Update
![Features](features/Polyphonic-Input--Output-green.svg)
![Features](features/Quantize-Yes-green.svg)<br>
The **Sample & Update (S&U)** module is not a traditional S&H/T&H/H&T module, although it can be configured to behave similarly. The module provides three sections — **Sample**, **Update**, and **Reset** — each with a manual button and a corresponding CV input. *Some general info regarding these modules are listed in [Common S&H features](commonShth.md).*

Between the large button and the CV input in each section, there is a small toggle button that controls how they behave. By default (**red**), each section operates in **Single mode**. In this mode, the button is momentary, and the CV input is treated as a **trigger**. Pressing the button—or receiving a trigger—causes the section to perform its action (e.g., sample) for a single processing cycle only, regardless of how long the button is held or the input signal remains high. Pressing the small toggle button switches the section to **Continuous mode** (**green**), where the large button becomes latched. In this mode, the section continues performing its action (e.g. sampling) for as long as the button is latched or the CV input remains high.

You can think of this module as a “delayed” sample-and-hold. When you press **Sample** or provide a Sample trigger, the input signal is captured and stored in memory—but the output does not necessarily update immediately. Instead, the output changes only when you press **Update** or provide an Update trigger. To emulate a traditional sample-and-hold, set the **Sample** section to **Single mode** (red), and set the **Update** section to **Continuous mode** (green) with the button latched. In this configuration, the module continuously outputs the last stored value, while new samples are written to memory only when triggered. To emulate a Track & Hold module, you set the sample section into continious mode, so it will continious update as long as the sample-input is high (or button is pressed/latched).

The **Reset** button or CV input clears the stored values (one per polyphonic channel) by setting them to 0V, and the output is updated accordingly. Through the context menu, you can choose a different reset value and optionally configure Reset to update memory only—meaning the output will not change until the next **Update** action is triggered.

**Action overview:**
* **Sample**: Input → Memory (stores one sampled value per channel)
* **Update**: Memory → Output (the output updates to the stored value; the last value is held while Update is inactive)
* **Reset**: Clears memory (sets it to 0V), then optionally updates the output

The input and output at the bottom of the module both support monophonic and polyphonic signals. For example, if you input a 4-channel polyphonic signal, the output will also be 4-channel polyphonic. However, the **Sample**, **Reset**, and **Update** inputs are monophonic. When a Sample gate or trigger is received, all channels are sampled simultaneously. *If a polyphonic signal is connected to any of these inputs, only the first channel is used to determine the gate/trigger state.*

![Screenshot of SampleAndUpdate](module/SampleAndUpdate.png) 

The 3-way **Mode** switch lets you choose between three different operating modes:
* **Value mode (default)**: The module samples and stores the actual input signal values.
* **Gate mode**: Similar to Value mode, but the input is interpreted as a gate signal. By default, input values ≥ 1V are treated as a high gate. When the output is updated, high gates are output as 10V and low gates as 0V. These thresholds and output levels can be adjusted in the context menu—for example, allowing you to invert gates by outputting high gates as 0V and low gates as 10V.
* **Trigger mode**: Instead of storing signal values, the module detects whether any triggers occur during the sampling period (and count the number of detected triggers). When an update is requested (via the **Update** button or input), the module outputs a single trigger if any triggers were detected, or 0V if none were detected.

However using the context menu you can set the module to count the number of input-triggers it shold detect (1-16). No matter what you set the trigger-count to (e.g. 4), it will only output a sigle trigger once the specified count have been achieved (or more). By default the internal count of input triggers is automatic cleared when the module recieves a reset- (no matter internal count), or an update-input while the specified count have not yet been accomplished. Hence if requested count have not yet been achieved, it will not clear the count. 

You can customize this behavior in the context menu (Only on Reset):
* **Only on Reset**: Trigger count is only cleared on Reset.
* **Only on Update (count reached)**: Trigger count is only cleared on update, when desired count have been reached.
* **Reset and Update (count reached)**: Trigger count will both clear on Reset, and update, when desired count have been reached.

If the **Update** button is not pressed/latched, or you are not supplying a continuous high gate, the module can detect more triggers than the specified count. For example, if **Sample** is latched but **Update** is only triggered occasionally (e.g., by a clock signal), the module will only evaluate the trigger count when an update is requested. The configured count acts as a **minimum threshold**—so if it is set to 4, the module will output a trigger whenever the detected count is 4 or higher at the moment an update occurs.

**Warning**: If you in trigger mode set the trigger-count to only clear on reset, and you set the update-button/input to operate in continious mode, the output will keep generating a stream of output-triggeres as soon as the desired count have been reached. 

**TIP**: While in Gate mode the module can function as a Data Flip-Flop. Where you input the "data signal" into the sample-input, and press the sample button latched (so it samples all the time). You then input your trigger signal into the "Update" CV-input (in trigger mode). Whenever the the module receives an Update trigger it will update the output, and because we set the module Mode-switch to Gate-mode the sample Data-signal is output as a gate.

**TIP**: While in Trigger mode the module can function as a clock-divider (though outputting a trigger). Used as such you'll latch both Sample- and Update-buttons to on, connect your clock-input to the IN-port and set trigger-count to the desired division (e.g. 3). In this configuration the OUT-port will generate a trigger-signal each time the module have observed 3 clock signals ("read as triggers"). 

Other S&H modules: [S&H/T&H-2](SHTH2.md), [S&H/T&H-2x4](SHTH2x4.md), [Stack](Stack.md), [Stack expander](StackExpander.md), [Stack bank expander](StackBankExpander.md).

[Go back to modules overview](plugin.md#modules)
