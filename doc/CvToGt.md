# CV to Gate
![Features](https://img.shields.io/badge/Polyphonic-Input--Output-green.svg?style=flat-square)<br>
This 2HP module checks whether the CV input signal falls within a specified range. The range is defined using the **Min** and **Max** knobs and their corresponding CV inputs. Each of these sections includes a button to determine whether the Min/Max values are **inclusive** (i.e., part of the range). At the bottom of the module, you’ll find four outputs, which by default all produce gates:
+ **Rng**: Outputs a high gate while the input is within the specified range
+ **!Rng**: Outputs a high gate while the input is outside the specified range
+ **Ab/Mx**: Outputs a high gate while the input is above the Max value
+ **Blw/Mn**: Outputs a high gate while the input is below the Min value

## Diff mode
By default, the module operates in **Gate mode**, as described above. Using the 2-way Gate/Diff switch, you can change it to **Diff mode**. In Diff mode, the last two outputs no longer produce gates. Instead:
+ **Ab/Mx** outputs the difference between the input value and the Max value
+ **Blw/Mn** outputs the difference between the input value and the Min value

By default, these differences are **signed**, but you can switch to **absolute values** via the context menu.

## When Min is greater than Max (**Min Mode**)
By default, if the Min value exceeds the Max value, the module enters an **error state**:
+ The light next to **Min** turns **red**
+ The **!Rng** indicator also lights red
+ The **!Rng** output remains constantly high, indicating an invalid range

You can change this behavior in the context menu by enabling **Auto/Swap**. In this mode:
+ If Min > Max, the values are automatically swapped internally
+ The module always treats the lower value as Min and the higher as Max
+ The Min indicator lights **green** (instead of red) to show the swap is active
+ The **!Rng** indicator becomes dim, and the output behaves normally (only high when the input is outside the valid range)

![Screenshot of CV to Gate](module/CvToGt.png)

[Go back to modules overview](manual.md#modules)