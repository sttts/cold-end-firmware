Installation of SSD1306 or SH1106 OLED display:

1) Import Adafruit-GFX-Library and either Adafruit_SSD1306 or Adafruit_SH1106
   depending on which OLED display you plan to use.

2) Uncomment (define) the appropriate OLED display

3) Flash your ColdEND, vent the system, make sure that the mist knob is fully turned clockwise
   for maximum lubrication and turn the spit knob fully counterclockwise to deactivate spit mode.
   Meter your lubrication output over 15 minutes and multiply the value by 4 to get the quantity in milliliter per hour.
   Edit row 82 (ml_per_hour) to this value and reflash your ColdEND.


ATTENTION: Refreshing the screen takes some milliseconds and freezes the loop for this time.
In rare cases it may happen that the coolant or spit value jumps between numbers.
To avoid a jumping value to interrupt the loop, turn the potentiometer slightly into one direction.

NOTE: If your stepper speed is too high and you have already set your microsteps to maximum,
decrease "mist_min_flow_rate" and "mist_max_flow_rate" since they limit the stepper timing.
After changing any of these values, you will need to repeat step 3 to calibrate the displayed quantity.
