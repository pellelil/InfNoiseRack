# Tiny Logic Comparator-2
![Features](features/Polyphonic-Input--Output-green.svg)<br>
This compact 2 HP module consists of two identical sections, each featuring four inputs and a single output. At the top of each section, a three-way switch allows you to select between the three most common logic operations: AND, OR, or XOR. Adjacent to this switch is a small button that toggles between normal and inverted logic. When this button is lit red, inverted logic is enabled, effectively transforming AND into NAND, OR into NOR, and XOR into XNOR. Below the three-way switch, there are four input ports, each of which can be individually inverted via the context menu. When an input is inverted, a small red light next to the corresponding input port will illuminate. However, if an input is not connected, it is simply ignored, meaning inverting a non-connected input has no effect. At the bottom of each section, a single output port produces a high gate (default 10V) when the logical operation evaluates as true or a low gate (default 0V) when false. These output voltage levels can be adjusted in the context menu. *Some general info regarding the Compare-modules are listed in [Common compare features](commonCompare.md).*

![Screenshot of Tiny Logical-Compare 2](module/TinyLCMP2.png)

The OR and XOR operations essentially function as a **"count of high inputs"**. By default, the OR operation outputs a high gate when one or more inputs are high, while the XOR operation produces a high gate when exactly one input is high. However, the context menu allows you to adjust this count, changing how many inputs must be high before the module outputs True/High. For example, increasing the count to 2 means that:

+ The OR operation will only output high when two or more inputs are high.
+ The XOR operation will only output high when exactly two inputs are high (no more, no less).

When the count is changed from its default value of 1, a small red indicator light above the XOR label will turn on, signaling that a non-default count has been set. However, you will need to open the context menu to see the exact count value that is selected. Since OR and XOR cannot be selected simultaneously, there is only one count setting in the menu, which applies to both operations. If the module is set to AND mode, this count setting is ignored, and the indicator light remains dimmed, even if a custom count has been selected.

**TIP**: Since the number of high inputs is counted, the logical operation is applied based on the selected count. When set to OR mode, the module outputs high when the number of high inputs is equal to or greater than the selected count. In XOR mode, the module only outputs high if the number of high inputs exactly matches the selected count. Enabling inverted logic (NOR mode) while using OR reverses this behavior—the module will only output high when fewer than the specified number of inputs are high. For example, if the count is set to 3 in NOR-mode, the module will output high only when fewer than three of the inputs are high.

Other compare modules: [Logic Comparator-2](LCMP2.md), [Logic Comparator-6x2](LCMP6x2.md), [Value Comparator-1](VCMP1.md), [Value Comparator-2 Mk I](VCMP2I.md), [Value Comparator-2 Mk II](VCMP2II.md).

[Go back to modules overview](plugin.md#modules)
