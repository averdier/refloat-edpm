# EDPM Speed Limit

This version of Refloat has a mandatory **Speed Limit**. It aims to make a self-balancing board (OneWheel-type) meet the 25 km/h maximum design speed of an EDPM (_Engin de Déplacement Personnel Motorisé_) in France.

- Above the limit, the board raises its nose (a pushback). This forces the rider into an uncomfortable stance until they slow down. It works like the tiltback on electric unicycles.
- The limit can't be disabled, and it can't be set above **25 km/h** in any way: VESC Tool, the package UI, third party apps, runtime commands, old configurations. It can be set lower, down to 10 km/h.
- The pushback grows steeper with the overspeed, by 1° per km/h, up to 10°.
- **The motor power is never cut or limited.** On a self-balancing vehicle, doing so makes the board nosedive and the rider fall.

> [!IMPORTANT]
> This software feature alone doesn't make a board a compliant EDPM. See [Limitations](#limitations-and-residual-risks) and [Validation](#validation).

## Regulatory Background (France)

> [!WARNING]
> Regulations change: check the current consolidated texts on [Légifrance](https://www.legifrance.gouv.fr) before relying on them.

**Definition**: Code de la route, article R311-1, point 6.15:

> « Engin de déplacement personnel motorisé : véhicule sans place assise, conçu et construit pour le déplacement d'une seule personne et dépourvu de tout aménagement destiné au transport de marchandises, équipé d'un moteur non thermique ou d'une assistance non thermique et dont la vitesse maximale par construction est supérieure à 6 km/h et ne dépasse pas 25 km/h. »

- The French ministry explicitly lists _monoroues_ (single-wheel vehicles) among EDPMs.
- The definition refers to the _gyropode_ of Regulation (EU) No 168/2013, art. 3 (71), which covers self-balancing single-wheel vehicles.
- No official text names "OneWheel" boards explicitly.

**Penalties related to the speed:**

| Article | Offence | Penalty |
|---|---|---|
| R321-4-2 | Riding on public roads an EDPM whose _vitesse maximale par construction_ exceeds that of R311-1 6.15 | 5th class fine |
| R317-23-1 | Using an EDPM fitted with a device allowing it to exceed the regulatory speed, or modified to that end (_débridage_) | 4th class fine |
| L317-1, L317-5 | Professionals: speed limiting devices, and selling or promoting devices that exceed the limits | Criminal offence |

**Other EDPM requirements:** these are out of the scope of this software and must be handled by the vehicle as a whole.

- Lights: R313-4 and R313-5.
- Reflectors: R313-18 to R313-20.
- Audible warning device: R313-33.
- Braking system: R315-7, with the _arrêté du 21 juillet 2020_.
- Rider rules: minimum age 14 since 1 September 2023 (R412-43-3), insurance, and where to ride (R412-43-1).

**Standard NF EN 17128:2020** (Personal Light Electric Vehicles):

- It covers self-balancing vehicles. Class 4 is self-balancing vehicles up to 25 km/h.
- A ±10 % tolerance on the measured maximum speed is reported (clause 8.2), to be checked against the text of the standard.
- No explicit anti-tampering requirement was identified.
- A revision of the standard, split into parts, is in progress.

## Physics: Why a Pushback and Never a Power Cut

A self-balancing board is an inverted pendulum: the motor torque is what keeps the board, and the rider, from falling.

- The balance loop (`pid.c`) commands a motor current to bring the board pitch to a **setpoint**.
- When the rider leans forward, the board accelerates to bring the nose back up.
- If the torque is cut or limited while the rider leans forward, nothing holds the nose up any more: **nosedive**. Refloat warns about this at the duty cycle limit: _"Reaching 95% Duty Cycle and pushing beyond WILL result in a nosedive."_
- A US CPSC recall of Onewheel boards in 2023 cites boards that _"can stop balancing the rider if the boards' limits are exceeded"_.

Therefore, the only safe lever is the **setpoint**.

**How a pushback slows the rider down:**

1. Raising the setpoint (nose up in the direction of travel) makes the controller apply some forward torque to lift the nose, then hold it there.
2. Standing on a deck tilted backwards is uncomfortable. The natural reaction is to shift the weight back, aligning with the deck.
3. That shift moves the center of mass behind the wheel, so the controller has to brake to hold the setpoint.

The balance authority is preserved during the whole process.

**Limits of a pushback:**

- A pushback is not a physical speed cap. A rider can "lean through" it, which is reported for electric unicycles and for Onewheel boards. Future Motion: _"never lean forward into push back"_.
- That is why the Speed Limit grows with the overspeed (+1°/km/h). The further the rider pushes, the steeper and less tenable the stance becomes.
- The angle is capped at 10°, which is the default Low Voltage Pushback angle (the steepest pushback Refloat applies by default). Steeper angles bring the tail closer to the ground. The actual tail-drag angle depends on the board geometry and wasn't found documented.
- The pushback is rate limited, so the setpoint never jumps. A fast setpoint change at speed causes a jolt (_"fast pushback can be dangerous!"_).
- During the rise of the pushback, the speed can still increase. Example: with the default 5° at 3°/s, the base angle takes about 1.7 s to build up. At an acceleration of 0.5 m/s², for example a gentle downhill, the speed grows by about 3 km/h in that time.

## Design

The logic lives in [speed_limit.c](/src/speed_limit.c) and is called from the main loop in [main.c](/src/main.c). Before this change, a speed pushback existed but it was optional and could be bypassed.

### Speed Limit Threshold

The Refloat Cfg option `tiltback_speed` (**Speed Limit (EDPM)**) is the threshold.

- The firmware always derives the effective value with `speed_limit_threshold()`:
  - `0` (formerly "disabled") and values above 25 give **25 km/h**;
  - values below 10 give 10 km/h.
- This happens in the control loop on every iteration, independently of how the configuration was written.
- The stored configuration is also corrected whenever it's loaded or written, so that VESC Tool and apps show the value in effect:
  - EEPROM read (startup, restore, lock);
  - VESC Tool write (`set_cfg`);
  - `COMMAND_TUNE_TILT` from apps.
- The XML range is 10–25 km/h, and the default is now 25 (it was 0, disabled).
- The configuration format and signature are unchanged, so existing configurations load without loss.

### Pushback Angle and Rates

The existing options are reused, clamped for the Speed Limit only. The Duty Cycle Pushback keeps using them unchanged.

| Option | Use in the Speed Limit | Bounds | Rationale |
|---|---|---|---|
| Pushback Angle (`tiltback_duty_angle`) | Base angle at the threshold | 3°–10° | 0° would disable it, 10° is the default LV Pushback |
| Pushback Speed (`tiltback_duty_speed`) | Rate of the rise | 1–5 °/s | 0 would disable it, fast pushback is dangerous |
| Return To Level Speed (`tiltback_return_speed`) | Rate of the release | 1–5 °/s | 0 would never release it |

The target angle is computed as follows:

```
target = min(base_angle + 1°/(km/h) * (speed - threshold), 10°)   when speed > threshold
target = 0                                                         otherwise
```

With the defaults, this gives 5° just above 25 km/h, 7° at 27 km/h and 10° from 30 km/h.

### Speed Measurement

The package uses `max(|mc_get_speed()|, |ERPM| * erpm_to_speed)`.

- `erpm_to_speed` is computed in [motor_data.c](/src/motor_data.c) from the motor configuration, with the same formula as the firmware: `ERPM / pole pairs / 60 * π * wheel diameter / gear ratio`.
- On firmware 6.06+, `mc_get_speed()` can be overridden from LispBM (`override-speed`), so it can't be trusted alone.

### Integration in the Setpoint

The other pushbacks (duty, voltage, temperature, BMS) and the setpoint adjustments (centering, reverse stop) form the _base setpoint_. The Remote, Nose Angling and the Tilts add _offsets_ to it.

1. **Base setpoint**: in the direction of travel, the higher of the base setpoint and the speed pushback is used.
   - Pushbacks don't add up: for example, 10° of Low Voltage Pushback wins over 6° of speed pushback.
   - A pushback configured to 0° can't mask the Speed Limit.
   - Reverse Stop gets this setpoint as its starting point, like it got the former speed pushback.
2. **Remote and Nose Angling** are controlled by the rider or the configuration, and can't lower the nose against the pushback:
   - The Remote tilt (also driven by apps via `COMMAND_REMOTE`) and a negative Constant or Variable Tiltback are neutralized when they lower the nose. When they raise it, they're kept.
   - The neutralization ramps in with the pushback angle (fully neutralized from 3°), so the setpoint stays continuous.
   - The pitch fault and Quick Stop use the neutralized Remote tilt to decide whether the Remote is tilting the board (more than 30°).
3. **Tilts** (ATR, Torque Tilt, Brake Tilt, Turn Tilt) are kept as they are, like with the other pushbacks. They compensate for the slope and the load:
   - On a downhill, the deck-to-ground angle is the setpoint plus the slope. Neutralizing ATR there would raise the nose relative to the ground, towards tail drag.
   - Torque Tilt lowers the nose while braking, which is what the rider does when obeying the pushback.

**Other behaviors:**

- **Direction**: the pushback follows the direction of travel, like the other pushbacks. The direction only changes once the pushback is fully released, so it never flips sides abruptly. When the board moves the other way, the remaining pushback is released at the maximum rate (5 °/s), so the pushback applies in the new direction within 2 s at most and doesn't hold back the other pushbacks there.
- **Wheelslip**: the wheel speed isn't the speed of the board, so the pushback is held as is until traction is regained. This prevents both false triggering and release. At the duty cycle limit, the pushback is released instead, like the other pushbacks.
- **Flywheel mode**: not applied. The board stands on its nose, it can't be ridden, and the mode aborts as soon as a footpad sensor is engaged.
- **Alerts**: while above the limit, if no other pushback or alert condition is active, the state reports `SAT_PB_SPEED`, at the same place as the former speed pushback. This shows as "pushback: speed" in the package UI and plays the Duty/Speed haptic feedback (audible tone on firmware 6.05+, enabled by default).
- **Remote move** (riderless board movement in the Ready state): Maximum Move Speed is kept at or below 25 km/h. Its configuration range is already 0–10 km/h. This relies on correcting the stored configuration.
- **Realtime data**: a new item `speed_limit.angle` gives the current pushback angle. It's sent, not recorded.

## Limitations and Residual Risks

1. **Not a physical cap.** As explained above, the speed can exceed the threshold transiently, and a rider determined to lean through the pushback can go faster. The progressive angle makes it harder and harder, but doesn't prevent it.
2. **Speed calibration.** The speed is computed from the Wheel Diameter, Motor Poles and Gear Ratio of the motor configuration.
   - Anyone with VESC Tool can change them, as can LispBM with `conf-set`. A wrong calibration shifts the actual limit.
   - The package can't lock the motor configuration. The firmware only offers compile-time options (`HW_MCCONF_READ_ONLY`), and the firmware can be reflashed anyway.
   - An EDPM product needs a hardware and firmware integration that prevents this.
3. **Tire.** The effective rolling diameter varies with tire pressure, wear and load. The threshold should take the measurement error into account.
4. **Modified software.** Anyone can rebuild this package or the firmware without the limit. This is _débridage_ (R317-23-1).
5. **Duty cycle.** On setups whose duty cycle limit is reached below 25 km/h, the Duty Cycle Pushback triggers first. This is the existing behavior.
   - Near the duty cycle limit, the existing wheelslip handling also suspends the pushbacks. The wheelslip state lasts as long as the duty cycle stays at 85 % or more.
   - The board has to reach the speed limit well below the Duty Cycle Threshold (80 % by default).
6. **Slopes.** The Tilts are not neutralized (see above). A strong ATR downhill configuration reduces the pushback felt on a downhill.
7. **Remote move.** The riderless Remote move uses `mc_get_speed()`, which LispBM can override. The move is torque limited (10 Nm) and only happens when nobody rides the board.
8. **Not validated on a vehicle.** The bounds (3–10°, 1°/km/h, 1–5 °/s, 10 km/h minimum) are engineering choices derived from the existing Refloat defaults. They need to be validated by riding tests.

## Validation

Suggested procedure for an integrator, on a closed area with protective equipment:

1. Calibrate the speed: Wheel Diameter measured under load (rolling circumference / π), Motor Poles and Gear Ratio (1 for a hub motor). Compare the reported speed with a GNSS speed at a steady speed.
2. Check in Refloat Cfg that the **Speed Limit (EDPM)** shows 25 km/h, and that it can't be set above 25 km/h. A value out of range coming from an app or an old configuration (e.g. 0) must show as corrected after reading the configuration back.
3. Ride and record the `speed`, `setpoint` and `speed_limit.angle` realtime values from the package UI.
   - Below the threshold: `speed_limit.angle` stays 0.
   - Above it: it rises at the configured rate up to the progressive target, and the pushback is felt.
4. On flat ground, going forward, repeat while trying to cancel the pushback with a Remote tilt or a negative Constant Tiltback. Once `speed_limit.angle` is 3° or more, the `setpoint` must not go below it, apart from the Tilts (keep ATR and Torque Tilt in mind when reading the values). Then repeat on a downhill.
5. Measure the maximum speed reached in normal riding, on flat ground and downhill. Adjust the threshold to keep a margin under 25 km/h that covers the overshoot and the measurement error.
