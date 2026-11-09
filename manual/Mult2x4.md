# Mult2x4
![Features](features/Polyphonic-Input--Output-green.svg)
![Features](features/Quantize-Yes-green.svg)<br>
This 2 HP module have 2 sections, each with an input and 4 outputs, where the 4 outputs clones the inputs. If you don't input a signal into the input-port of the lower section, it will normalize to the input of the upper section. Hence the module easily doubles as mult with 1 input and 8 outputs. If the input is polyphonic (up to 16 channels), all outputs will preserve that polyphony. By default, all outputs are clipped to -12V/+12V to prevent excessive signal levels. However, this clipping can be adjusted or disabled via the context menu. *Some general info regarding these modules are listed in [Common merge/mult features](commonMergeMult.md).*

![Screenshot of Mult-2x4](module/Mult2x4.png) 

**TIP**: Typical in VCV when a module sets its outputs, other modules (connected via cables) wont see this signal until they are processed at the next cycle, hence while not its designed purpose, a Mult can also be used as a 1 cycle delay (in few/edge cases this might be exactly what you need).

Other merge/mult modules: [Merge2x4](Merge2x4.md), [Merge/Mult-4](MergeMult4.md).

[Go back to modules overview](plugin.md#modules)
