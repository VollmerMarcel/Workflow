# Car Wear Prognosis Engine — Whitepaper

**A single-case, one-year predictive model for gasoline vehicles**

| | |
|---|---|
| Document version | 0.3 — draft for review |
| Date | 2026-10-22 |
| Changes | Model and Monte Carlo harness separated: the model produces one run for one car; the harness aggregates runs |
| Status | Model definition, no implementation yet |
| Scope | One specific car, one specific user, one specific location |
| Model output | One run: part states, events, and costs for a single car |
| Harness output | Distribution of total cost of ownership, part wear, and failures across runs |

---

## Executive summary

This document specifies a simulation model that estimates how a specific gasoline car will wear and what it will cost over a horizon of one year. The car, the driver, and the location are fixed. A model run simulates one car over one horizon and produces one outcome: the state of every part, the events that occurred, and the costs that followed. The model contains no Monte Carlo loop and computes no statistics. Monte Carlo is performed by a separate harness that calls the model repeatedly with different resolved inputs and run seeds, and aggregates the runs. The harness is a consumer of the model, not part of it.

The model has four layers. A climate process generates weather. A trip generator turns the driver's weekly rhythm into trips represented by a handful of sufficient statistics. A set of component models turns those trips into wear, replacements, and failures. An accounting layer turns events into total cost of ownership. Every numeric parameter is read from a configuration file, and a run is written as TOML plus CSV.

The document is deliberately explicit about what is not modelled: brake failure, accidents, hybrids, and a detailed paint-surface model. It is equally explicit that the constants are engineering priors, not calibrated truths, and that no coefficient may be tuned to reproduce an expected output.

---

## Contents

1. [Purpose and scope](#1-purpose-and-scope)
2. [Scope decisions](#2-scope-decisions)
3. [Modeling discipline](#3-modeling-discipline)
4. [Problem statement](#4-problem-statement)
5. [Simulation architecture](#5-simulation-architecture)
6. [Uncertainty model](#6-uncertainty-model)
7. [Weather and climate zones](#7-weather-and-climate-zones)
8. [Trip generation](#8-trip-generation)
9. [Vehicle subsystem models](#9-vehicle-subsystem-models)
10. [Fuel consumption](#10-fuel-consumption)
11. [Maintenance and replacement](#11-maintenance-and-replacement)
12. [Failure model](#12-failure-model)
13. [Monetary model](#13-monetary-model)
14. [Output contract](#14-output-contract)
15. [Monte Carlo harness](#15-monte-carlo-harness)
16. [Parameterization](#16-parameterization)
17. [Validation](#17-validation)
18. [Limitations](#18-limitations)
19. [Roadmap](#19-roadmap)
20. [Appendix A — Notation](#appendix-a--notation)
21. [Appendix B — Model constants](#appendix-b--model-constants)
22. [Appendix C — Worked example](#appendix-c--worked-example)
23. [Appendix D — Event ordering](#appendix-d--event-ordering)
24. [Appendix E — Resolved design decisions](#appendix-e--resolved-design-decisions)
25. [Appendix F — Sources and calibration anchors](#appendix-f--sources-and-calibration-anchors)

---

## 1. Purpose and scope

The engine answers one question:

> Given car X, user Y, and location Z, how does the car perform over the next year, and what does that cost?

It produces, for one run, the outcome of one car over one horizon:

- wear indicators for every replaceable part in the model,
- replacement events and their dates,
- failure events,
- fuel consumption and the other cost components,
- the monthly shape of cost and wear.

The engine is a decision aid. It is not a warranty, a residual-value appraisal, or a substitute for an inspection.

The model is deliberately **case-based**: the car, the user, and the location are fixed inputs. There is no population of drivers and no fleet aggregation. A run is one stochastic realization of that case. The predictive distribution is produced by the Monte Carlo harness, which varies the uncertain inputs and the run seed and aggregates the runs.

---

## 2. Scope decisions

These decisions are binding for version 0.1.

| Topic | Decision |
|---|---|
| Horizon | 365 days by default, configurable |
| Start date | Required input; seasonality matters |
| Powertrain | Gasoline only; no hybrid, plug-in hybrid, or electric |
| Transmission | Manual or automatic |
| Brake failure | **Not modelled**; brakes only wear and are replaced |
| Accidents | Not modelled; no crash cascade |
| Paint | Condition indicator only; low monetary weight; no large value tail |
| Primary monetary metric | Total cost of ownership (cash operating cost) |
| Depreciation | Reported separately, excluded from TCO |
| Parameter source | Every numeric input comes from a TOML configuration file |
| Model boundary | One run simulates one car for one horizon; no replication loop, no aggregation, and no statistics inside the model |
| Monte Carlo | A separate harness that resolves uncertain inputs, assigns run seeds, calls the model, and aggregates the runs |
| Model output | One run: part states, an event log, and cost components, as TOML plus CSV |
| Harness output | Aggregate distribution as TOML, plus one CSV row per run |
| Calibration | No tuning to expected outputs; constants are priors |

Three consequences deserve emphasis.

**Brake failure is out of scope.** Brake pads and discs are consumables. They wear, reach a replacement threshold, and are replaced. No safety-critical brake failure event is sampled.

**Paint does not carry a large cost tail.** Paint is modelled as a slowly accumulating condition index with stone chips, hail, parking dings, and rust onset. Its monetary contribution is small by design. This is a scope choice, not a claim that paint is irrelevant to owners.

**The model and the harness are separate.** The model is a single stochastic simulation of one car. Monte Carlo is a way of using the model, not a feature inside it. A run is reproducible from its resolved inputs and its run seed. The harness may change its sampling strategy or its statistics without touching the physics.

---

## 3. Modeling discipline

The engine is built under five rules.

**Rule 1 — Structure from physics, constants from independent sources.** A model term exists because a physical mechanism exists, not because it helps reproduce an expected number. Coefficients come from first principles, manufacturer data, or explicit engineering estimates.

**Rule 2 — No fitting to expected outputs.** If the model predicts 5.5 litres per 100 km where a reviewer expects 7, the correct response is to examine the model and its inputs, not to multiply a coefficient until the number matches. Calibration against real data is a separate, documented activity. Version 0.1 has no calibration data and must say so.

**Rule 3 — Every number is configurable.** The code contains only structural constants: unit conversions, the number of days per year, and mathematical constants. Every physical, behavioral, and economic parameter is read from a configuration file, can be overridden by the user, and is echoed in the output.

**Rule 4 — Prefer few, interpretable parameters.** A parameter that cannot be explained in one sentence does not belong in the model. Where two formulations fit equally well, the one with fewer parameters wins.

**Rule 5 — Report uncertainty honestly.** A run is one outcome with explicit inputs. The harness reports the distribution across runs with explicit assumptions. Neither is a point prediction with decorative decimals.

### 3.1 Parameter provenance

Every parameter carries a provenance class in the documentation and, where practical, in the configuration schema.

| Class | Meaning | Example |
|---|---|---|
| Physical constant | Universal or near-universal | Energy density of gasoline, air density, gravity |
| Manufacturer specification | Vehicle-specific | Mass, drag area, engine displacement |
| Engineering estimate | Derived from mechanics or dimensional reasoning | Pad wear per megajoule, cold-start wear equivalent |
| Literature prior | Published failure or wear statistics | Annual failure rate of a water pump |
| User-supplied | Measured or declared by the car owner | Pad thickness, annual distance, driving style |

A nominal value is a prior. It is not a calibrated truth. The whitepaper records nominal values so that the model can be audited and later calibrated, not so that it can be tuned.

---

## 4. Problem statement

### 4.1 The case

A case is a fixed tuple:

```
case = ( vehicle, currentState, user, location, maintenancePolicy, economics, startDate )
```

- **Vehicle**: mass, drag area, engine, transmission, drivetrain, component specifications, odometer, age.
- **Current state**: measured or estimated condition of every modelled part, with an uncertainty.
- **User**: commuting pattern, errand and leisure trips, long trips, driving style, maintenance adherence, parking situation.
- **Location**: climate zone, road quality, traffic density, road-salt practice.
- **Maintenance policy**: service intervals, deferral behavior, replacement thresholds, part quality.
- **Economics**: fuel price, regional price index, part and labor costs, downtime value.
- **Start date**: the calendar date on which the horizon begins.

### 4.2 The model output

A run maps one resolved case and one run seed to one outcome:

```
run = model( resolvedCase, runSeed )

outcome = ( part states at horizon,
            replacement events and dates,
            failure events,
            fuel consumption,
            cost components,
            monthly timeline )
```

The resolved case is the case with every uncertain entry replaced by a concrete value, including the car's latent current state and its frailty factor. The run seed drives the weather, the trip realization, and the timing of rare events inside that run.

A run is one realization, not a distribution. The predictive distribution over outcomes is produced by the Monte Carlo harness, which resolves the uncertain inputs, assigns a run seed to each run, calls the model, and aggregates:

```
outcomes = { model( resolve( case, sample ), seed( sample ) ) for each sample }
```

### 4.3 Definitions

**Wear indicator.** A physical quantity or a normalized index describing the remaining life of a part. Physical quantities are preferred: millimetres of pad, millimetres of tread, litres of fuel. Normalized indices are used only where no practical physical measurement exists, such as the engine or the exhaust system.

**Replacement.** A planned event triggered by a maintenance threshold, a service schedule, or an inspection. A replacement is a cost, not a failure.

**Failure.** An unplanned event that disables or degrades the part before its planned replacement. A failure is both a cost and a risk event. Brake failure is excluded by scope.

**TCO.** Cash operating cost over the horizon: fuel, scheduled maintenance, wear replacements, unplanned repairs, and consequential costs such as towing and downtime. Depreciation is reported separately because it is a change in asset value, not a cash outflow.

**Horizon.** The number of simulated days, default 365. The start date determines which seasons the car experiences.

---

## 5. Simulation architecture

### 5.1 The run loop

A run is one simulation of one car over one horizon. It has no replication loop and no aggregation step; those belong to the harness.

The model, for one run:

```
run( resolvedCase, runSeed ):
    initialize state from resolvedCase
    for each day d:
        generate weather for day d
        apply seasonal tire swap if scheduled
        generate trips for day d
        for each trip:
            update fuel
            update component wear
            update cold-start state
        apply daily aging (battery, paint, tire pressure, exhaust)
        apply scheduled maintenance
        apply threshold replacements
        sample failure events
        record monthly snapshot
    return the run result
```

The harness, for many runs (Section 15):

```
monteCarlo( case, numberOfRuns, masterSeed ):
    for runIndex in 1 .. numberOfRuns:
        sampleSeed  = hash( masterSeed, runIndex, "run" )
        resolvedCase = resolve( case, masterSeed, runIndex )
        results[ runIndex ] = run( resolvedCase, sampleSeed )
    aggregate( results )
```

`resolve` replaces every uncertain case entry — the latent current state, the car-level frailty, and any sampled parameter prior — with a concrete value, so the model itself always receives a fully determined case.

The day is the smallest time step. Maintenance and failures are resolved at the end of the day. This is an explicit approximation: a failure that would occur during a trip is assigned to the end of that day. The horizon is short and the number of trips per day is small, so the approximation has no material effect on annual cost or wear.

### 5.2 The state vector

| State | Unit | Reset by |
|---|---|---|
| Engine wear | equivalent kilometres | engine repair or replacement |
| Oil degradation | 0 to 1 | oil change |
| Oil level | fraction of capacity | oil change or top-up |
| Turbocharger thermal wear | 0 to 1 | turbocharger repair |
| Front and rear pad thickness | millimetres | pad replacement |
| Front and rear disc thickness | millimetres | disc replacement |
| Tire tread, four corners, per set | millimetres | tire replacement or rotation |
| Tire pressure, four corners | bar | pressure service |
| Battery state of health | 0 to 1 | battery replacement |
| Suspension wear | 0 to 1 | suspension repair |
| Clutch or transmission wear | 0 to 1 | clutch or transmission repair |
| Exhaust corrosion | 0 to 1 | exhaust repair |
| Catalyst degradation | 0 to 1 | catalyst replacement |
| Cooling-system wear | 0 to 1 | cooling repair |
| Paint damage | 0 to 1 | paint repair |
| Windshield chip count | integer | windshield replacement |
| Cumulative consumed life per part | part-specific | never |
| Cumulative distance and fuel | kilometres, litres | never |
| Event log | — | — |

The separation between **current installed state** and **cumulative consumed life** is essential. A part replaced in month four has a nearly new state at the horizon but a large cumulative wear. Both are legitimate outputs: one answers "what is on the car", the other answers "what did this user consume".

### 5.3 Sufficient statistics of a drive

A trip is not a trajectory. It is a point in a small statistical space:

```
drive = ( distance, meanSpeed, speedVariation, stopDensity,
          brakeOnsetSpeedFraction, lateralAcceleration,
          positiveAcceleration, idleFraction,
          routeClass, ambientTemperature, roadWet, roadSnowy )
```

Every subsystem model below depends only on these statistics and on the current state. This is what makes the simulation tractable and what makes "probabilistic rather than exact" a defensible modeling choice rather than a shortcut.

---

## 6. Uncertainty model

### 6.1 Two kinds of uncertainty

A set of runs mixes two fundamentally different things. The model itself does not separate them; the harness does, and it reports the decomposition.

| Layer | Source | Reduced by |
|---|---|---|
| Aleatory | Weather realization, trip lengths, event times within a run | Nothing |
| Epistemic | Unknown current state, uncertain hazard priors, car-level frailty | Measurement, data |

The harness keeps them separable. Epistemic sampling can be switched off to obtain the aleatory-only distribution, and the aggregate can show the variance decomposition:

```
totalVariance = meanOverSamples( withinSampleVariance ) + varianceOverSamples( meanOutcomePerSample )
```

Here a sample is one resolved case with one set of epistemic draws, and the runs sharing that sample differ only in their run seed. The first term is aleatory, the second is epistemic. This tells the user whether the answer is limited by luck or by missing information.

### 6.2 Where the variance actually is

A one-year horizon contains roughly 800 to 1200 trips. Independent per-trip noise averages out: the central limit theorem reduces its contribution to the annual total. If the model injected independent random noise into every wear increment, the annual wear distribution would be unrealistically narrow and failure probabilities would collapse to zero or one.

The variance must therefore be placed where it physically lives:

- **Weather path**: one realization per run, persistent across the year.
- **Trip realization**: Poisson and Bernoulli counts and log-normal distances, persistent within the year.
- **Event times**: Poisson chip, hail, and failure events.
- **Current state**: unknown pad thickness, oil age, battery health.
- **Car-level frailty**: one multiplicative factor per run that makes some cars systematically less reliable.

This is the single most important design rule in the uncertainty model.

### 6.3 Frailty

The harness draws a frailty factor for each run and passes it into the resolved case:

```
frailty = exp( normal( 0, sigmaFrailty ) )
```

The factor multiplies every random failure hazard for that run. It produces clustering: a car that suffers one unexpected failure is more likely to suffer another. Without frailty, failures are unrealistically independent and the cost tail is too thin.

`sigmaFrailty` is a configurable prior, nominally 0.45.

### 6.4 Uncertainty of the current state

The car's condition at the start is rarely known exactly. Every state entry has a nominal value and a standard deviation in the configuration. A value of zero means "known exactly". A large value means "unknown, use the prior". These are inputs to the harness; the model receives the resulting concrete state.

Sampling is truncated to the physically admissible range:

- thickness values are clipped to their replacement limit and the new-part value,
- health indices are clipped to 0 and 1,
- pressures are clipped to a safe range.

This matters because one year of pad wear is roughly 1 to 3 millimetres, while the uncertainty of an unmeasured pad is easily 1.5 millimetres. For an unmeasured car, the probability of needing pads this year is driven mostly by the prior, not by the simulation. The report exposes this explicitly so that the user knows which measurements would improve the answer.

### 6.5 Reproducibility

Each run uses independent random streams derived from its run seed:

```
streamSeed = hash( runSeed, streamName )
```

Streams exist for weather, trips, failure events, and costs. The same resolved case and run seed produce the same output, byte for byte, on the same platform. The run seed is printed in the run output.

The harness derives run seeds from its master seed and run index, so an entire study is reproducible from the case, the master seed, and the number of runs.

Independent streams also enable common random numbers: when comparing two scenarios, the same run seeds are reused, so the same weather and trip streams drive both scenarios and differences are attributable to the scenario rather than to sampling noise.

---

## 7. Weather and climate zones

### 7.1 Why a process, not a table

Independent daily sampling would destroy seasonal structure and autocorrelation. A cold snap would not persist for a week, and a wet spell would not soak the roads. Both matter for cold-start wear, battery failure, corrosion, and paint. Weather is therefore generated as a stochastic process whose parameters depend on the climate zone and the month.

### 7.2 Climate zone

A zone is a parameter set:

```
climateZone = ( monthlyTemperatureMean, monthlyTemperatureDeviation,
                monthlyPrecipitationProbability, monthlyUvIndex,
                temperatureAutocorrelation, precipitationPersistence,
                snowTemperatureThreshold, saltProbabilityOnFrostDay,
                annualHailDays, roadRoughnessIndex, trafficDensityIndex,
                humidityIndex, gravelRoadShare )
```

The zone is selected by name and may be overridden field by field. Presets are provided for temperate-maritime, continental, alpine, Mediterranean, Nordic, and arid climates. Presets are starting points, not authoritative climatologies.

### 7.3 Temperature

Temperature is an autoregressive process of order one around a monthly mean:

```
temperatureAnomaly(day) = phi * temperatureAnomaly(day - 1)
                          + sqrt(1 - phi^2) * monthlyDeviation(month) * normal(0, 1)

temperature(day) = monthlyMean(month) + temperatureAnomaly(day)
```

`phi` is the day-to-day persistence, nominally 0.7. The square-root term keeps the stationary standard deviation equal to the monthly deviation.

### 7.4 Precipitation

Precipitation is a two-state Markov chain with monthly stationary probability `p` and persistence `q = probability(wet | wet)`:

```
probability(wet | wet) = q
probability(wet | dry) = p * (1 - q) / (1 - p)
```

The second expression follows from stationarity and is clamped to the unit interval.

Snow occurs when precipitation coincides with a temperature below the snow threshold. A day is a road-salt day when it is snowy or frosty and the zone's salt probability is met. Road wetness persists for one day after precipitation.

### 7.5 Ultraviolet load and hail

Ultraviolet load is the monthly ultraviolet index reduced by cloud cover, which is correlated with precipitation. Hail is a Poisson event with an annual rate taken from the zone; a hail day applies a damage jump to paint.

### 7.6 Weather effects

| Weather condition | Engine | Brakes | Tires | Battery | Exhaust | Paint | Fuel |
|---|---|---|---|---|---|---|---|
| Low temperature | cold-start wear up, oil degradation up | — | winter compound | health down, cranking demand up | condensation up | — | cold enrichment up |
| Heat | thermal load up | fade risk up | wear up | aging up | — | UV damage up | air-conditioning load up |
| Rain | — | corrosion up | grip loss, wear up | — | — | wash-off | rolling resistance up, speed down |
| Snow and salt | — | corrosion up | — | — | corrosion up | salt damage and rust up | speed down, demand down |
| Hail | — | — | — | — | — | damage jump | — |

Driving behavior is also weather-dependent: rain and snow reduce mean speed and trip demand. This is part of the trip generator, not the subsystem models.

---

## 8. Trip generation

### 8.1 Trip purposes

Four purposes are modelled:

| Purpose | Timing | Distance distribution | Typical route class |
|---|---|---|---|
| Commute | Weekdays, probability `commuteDaysPerWeek / 5` | Log-normal around the commute distance | Mixed |
| Errand | Any day, Poisson rate | Log-normal around the errand distance | City |
| Weekend leisure | Saturday and Sunday | Log-normal around the weekend distance | Rural or mixed |
| Long trip | Any day, seasonal weight | Log-normal around the long-trip distance | Highway |

All distances are log-normal, parameterized by median and coefficient of variation:

```
sigmaLog = sqrt( ln(1 + coefficientOfVariation^2) )
muLog    = ln( median )
distance = exp( muLog + sigmaLog * normal(0, 1) )
```

Trip counts are generated as follows: commute trips are Bernoulli trials on weekdays with probability `commuteDaysPerWeek / 5`; errands use a Poisson rate of `errandTripsPerWeek / 7` per day; weekend leisure trips use a Poisson rate of `weekendTripsPerWeek / 2` on Saturdays and Sundays; long trips use a Poisson rate of `longTripsPerYear / 365` multiplied by the monthly weight. Overdispersion enters through the log-normal distance distribution and through epistemic uncertainty in the user's parameters, not through a negative-binomial count. A negative-binomial option is a roadmap item.

### 8.2 Seasonal weighting of long trips

Long trips are not uniform over the year. Each month has a weight; the daily rate is

```
longTripRate(day) = longTripsPerYear / 365
                    * monthWeight(month) / mean(monthWeight)
```

### 8.3 Weather modulation

```
tripDemandFactor = clamp( 1 - rainDemandReduction * wet
                            - snowDemandReduction * snowy, 0.3, 1.0 )
meanSpeed        = classMeanSpeed
                   * (1 + speedStyleSensitivity * (style - 0.5))
                   * weatherSpeedFactor
stopDensity      = classStopDensity
                   * (1 - stopStyleSensitivity * (style - 0.5))
                   * weatherStopFactor
```

`style` is the user's aggressiveness on a zero-to-one scale. Aggressive driving raises speed, acceleration, lateral acceleration, and brake onset speed, and slightly reduces the number of stops because the driver brakes later and harder.

### 8.4 Cold starts

A trip is a cold start when the engine has been off for longer than the cold-start threshold, nominally 120 minutes, or when it is the first trip of the day. The cold-start wear term is applied once per cold start, not per kilometre. This is the mechanism that makes many short trips more damaging than one long trip.

### 8.5 Derived drive statistics

```
stopCount        = stopDensity * distance
brakeOnsetSpeed  = meanSpeed * brakeOnsetFraction / 3.6     [metres per second]
brakeEnergy      = 0.5 * massVehicle * brakeOnsetSpeed^2 * stopCount
accelerationEnergy = 0.5 * massVehicle * (0.8 * meanSpeed / 3.6)^2 * stopCount
```

Brake energy is used for pad and disc wear. Acceleration energy is used for fuel. They are not interchangeable: braking energy is dissipated, acceleration energy is supplied by the engine.

---

## 9. Vehicle subsystem models

Each subsection states the state variables, the update rule, the parameters, the replacement threshold, and the failure treatment.

### 9.1 Engine

The engine is the most detailed subsystem. Its state is:

- mechanical wear, in equivalent kilometres,
- oil degradation, zero to one,
- oil level, as a fraction of capacity,
- turbocharger thermal wear, zero to one, when turbocharged.

#### 9.1.1 Cold-start wear

```
coldStartEquivalentDistance(temperature) =
    clamp( coldStartWearBaseKilometres
           * exp( -(temperature - 20) / coldStartWearTemperatureScale ),
           coldStartWearMinimumKilometres,
           coldStartWearMaximumKilometres )
```

Nominal values: base 5 equivalent kilometres at 20 degrees Celsius, temperature scale 20 degrees, maximum 20 equivalent kilometres.

#### 9.1.2 Mechanical wear per trip

```
loadFactor = 1 + engineLoadWearSensitivity * style^2
             * (0.5 + 0.5 * meanSpeed / 100)

oilWearFactor = 1 + oilWearSensitivity * oilDegradation^oilWearExponent

levelWearFactor = 1 + oilLevelStarvationWearFactor
                  * indicator( oilLevel < oilLevelCriticalFraction )

engineWearIncrement = distance * loadFactor * oilWearFactor * levelWearFactor
                      + coldStartEquivalentDistance * (1 + warmUpIdleFactor)
```

The engine wear state is the cumulative sum of these increments. Its normalized wear index is

```
engineWearIndex = engineWearEquivalentKilometres / engineDesignLifeKilometres
```

with a nominal design life of 250,000 kilometres.

#### 9.1.3 Oil degradation

Oil degrades through distance, cold starts, short trips, and calendar time:

```
oilDegradationIncrement =
      distance * oilDegradationPerKilometre
    + indicator(coldStart) * oilDegradationPerColdStart
    + indicator(shortTrip) * oilDegradationPerShortTrip
    + days * oilDegradationPerDay
```

The index is capped at one. Above one, the oil is considered exhausted and the wear multiplier and the oil-related hazard rise sharply.

#### 9.1.4 Oil consumption and level

```
oilConsumptionPer1000Kilometres =
    baseOilConsumption
    + wearOilConsumptionGrowth * engineWearIndex^1.5

oilLevel = oilLevel - oilConsumptionPer1000Kilometres * distance / 1000 / oilCapacityLitres
```

The level is refilled at every oil change. Below the critical fraction, the starvation hazard multiplier applies. This closes the loop from wear to consumption to level to accelerated wear.

#### 9.1.5 Turbocharger

For turbocharged engines, each trip adds thermal cycling damage:

```
thermalLoad = loadFactor * (1 + turboHeatSensitivity * meanSpeed / 100)
turboWearIncrement = turboWearPerCycle * thermalLoad^2
```

A hot shutdown after a high-load trip adds a fixed coking increment. The resulting turbo wear index feeds the turbocharger hazard and the engine wear multiplier.

#### 9.1.6 Engine failure modes

Engine failure is a set of competing risks. Each mode has a weight and an installed repair cost.

| Mode | Primary driver | Relative cost |
|---|---|---|
| Piston rings and cylinder bore | Mechanical wear, oil condition | High |
| Bearings | Oil degradation, oil starvation | High |
| Turbocharger | Thermal cycling, oil condition | Medium-high |
| Seals and gaskets | Age, thermal cycles | Low-medium |
| Ignition system | Distance, oil condition | Low |
| Timing chain or belt | Mechanical wear, oil condition | Medium |

If the engine is not turbocharged, the turbocharger weight is set to zero and the remaining weights are renormalized.

### 9.2 Brakes

Brake failure is explicitly excluded. Brakes are consumables.

#### 9.2.1 Pad wear

```
padTemperature = ambientTemperature
                 + brakeTemperatureRisePerMegajoulePerHour * brakeEnergy / durationHours

temperatureFactor = 1 + brakeTemperatureWearFactor
                    * max( 0, padTemperature - brakeTemperatureWearStart ) / 100

frontPadWear = brakeEnergy * frontBrakeEnergyShare
               * padWearCoefficient * temperatureFactor * padCompoundFactor

rearPadWear  = brakeEnergy * (1 - frontBrakeEnergyShare)
               * padWearCoefficient * temperatureFactor * padCompoundFactor
```

The brake energy is expressed in megajoules. The nominal pad wear coefficient is 0.010 millimetres per megajoule, which corresponds to roughly 50,000 kilometres of mixed driving for a 10-millimetre front pad. The front share is nominally 0.72. Load transfer shifts more braking to the front under hard deceleration; version 0.1 keeps the share constant and records the simplification.

#### 9.2.2 Disc wear

```
discWear = padWear * discWearRatio
```

The nominal ratio is 0.15 millimetres of disc per millimetre of pad. Discs are replaced when their thickness reaches the configured limit.

#### 9.2.3 Corrosion

Low-mileage cars in salt climates wear brakes by corrosion rather than friction:

```
padCorrosion = saltDays * brakeCorrosionPerSaltDay
```

This term is small per day but matters for a car that is driven rarely in winter.

#### 9.2.4 Brake fluid

Brake fluid is replaced on a time interval, nominally 24 months. Its condition is reported as remaining life; it has no failure event.

### 9.3 Tires

Four corners are tracked independently, for one or two tire sets.

#### 9.3.1 Tread wear

```
cornerLoadFactor = (massVehicle / referenceMass)^tireLoadWearExponent

pressureFactor = (referencePressure / effectivePressure)^tirePressureWearExponent

effectivePressure = measuredPressure * (293.15 / (273.15 + ambientTemperature))

lateralFactor = 1 + tireLateralWearSensitivity
                * (lateralAcceleration * (1 + style))^2

temperatureFactor = 1 + tireTemperatureWearSensitivity
                    * max(0, ambientTemperature - 20) / 20

surfaceFactor = 1 + wetSurfaceWearIncrease * indicator(wet)
                + gravelSurfaceWearIncrease * indicator(gravel)

treadLoss = distance * tireWearPerKilometre * cornerShare
            * cornerLoadFactor * pressureFactor * lateralFactor
            * temperatureFactor * surfaceFactor * tireCompoundFactor
```

Nominal tread wear is 1.4 times ten to the minus four millimetres per kilometre, which corresponds to about 6.4 millimetres over 45,000 kilometres.

Corner shares depend on drivetrain. Front-wheel drive biases wear to the front axle; rear-wheel drive biases it to the rear; all-wheel drive distributes it more evenly. The four shares sum to one.

#### 9.3.2 Pressure

Pressure leaks slowly:

```
pressure(day) = pressure(day - 1) - tirePressureLeakPerMonth / 30
```

Pressure is reset to the specified value at every inspection or oil change, depending on the maintenance policy. Pressure affects both wear and rolling resistance.

#### 9.3.3 Seasonal sets

If seasonal tires are enabled, two sets are tracked: summer and winter. The active set changes on the configured mount months. Each set has its own tread depths and age. Winter compound wears faster on dry roads and slower in cold conditions; the net effect is captured by a compound factor.

#### 9.3.4 Rotation and replacement

Tires are rotated at the configured distance interval. Rotation equalizes tread across the four corners. A pair is replaced when either tire on the axle reaches the tread limit, nominally 1.6 millimetres. Tires older than the age limit, nominally six years, are replaced regardless of tread.

### 9.4 Battery

The battery is a twelve-volt lead-acid starter battery. Its state is state of health, zero to one.

#### 9.4.1 Aging

```
stateOfHealth = clamp(
    1
    - batteryAgeFactor * sqrt(ageYears)
    - batteryHeatDamage * degreeDaysAbove25
    - batteryCycleDamage * startCount
    - batteryDeepDischargeDamage * deepDischargeCount,
    0.0, 1.0 )
```

Heat is the dominant aging factor. Degree-days above 25 degrees Celsius are accumulated daily.

#### 9.4.2 Cold-cranking failure

At each cold start, the required health depends on temperature:

```
requiredHealth = clamp( batteryRequiredHealthBase
                        + batteryRequiredHealthPerDegree
                          * (20 - ambientTemperature),
                        0.0, 0.95 )

probabilityNoStart = 1 / (1 + exp(-batteryFailureSteepness
                                  * (requiredHealth - stateOfHealth)))
```

A no-start event replaces the battery and records a failure. The event is more likely on the first cold morning of winter, which is an emergent and realistic behavior rather than a hard-coded seasonal rule.

### 9.5 Suspension

```
suspensionWearIncrement = distance * suspensionWearPerKilometre
                          * (1 + roadRoughnessSensitivity * roadRoughness)
                          * (1 + corneringWearSensitivity * lateralAcceleration)
                          * loadFactor
```

The wear index is zero to one. At inspection, a worn suspension is replaced as a planned event. A random failure hazard also exists, scaled by the wear index. Failure modes are lumped: worn dampers, bushings, and joints.

### 9.6 Clutch and transmission

```
clutchWearPerStart = manual ? clutchWearPerStartManual
                            : clutchWearPerStartAutomatic

clutchWearIncrement = clutchWearPerStart * loadFactor
                      * (1 + clutchStyleSensitivity * style)^2
```

The wear index is zero to one. It is consumed per start, not per kilometre, because clutch wear is dominated by launch events. A worn clutch is replaced at inspection or on failure. For automatic transmissions, the same index represents the lumped wear of the torque converter and clutch packs, with a much lower per-start coefficient.

### 9.7 Exhaust and catalytic converter

```
exhaustCorrosionIncrement =
      days * exhaustCorrosionPerDay
    + indicator(shortTripDay) * shortTripCorrosionFactor * exhaustCorrosionPerDay
    + saltDays * exhaustSaltCorrosionPerDay
    + humidityIndex * exhaustHumidityCorrosionPerDay

catalystDegradationIncrement =
      distance * catalystDegradationPerKilometre
    + oilBurned * catalystOilPoisoningFactor
```

Short trips produce condensation, which corrodes the exhaust from the inside. Burning oil poisons the catalyst. The latter is a feedback loop from engine wear to emissions cost.

An annual emissions inspection is performed at the configured month. If corrosion or catalyst degradation exceeds the configured threshold, the inspection fails and the affected component is replaced.

### 9.8 Cooling system

```
coolingWearIncrement = days * coolingWearPerDay
                       + trips * coolingWearPerThermalCycle
```

The coolant itself is replaced at its service interval, which resets the coolant age but not the mechanical wear of the pump, thermostat, and hoses. A random failure hazard is scaled by the wear index.

### 9.9 Paint

Paint is intentionally simple. It is a condition index, not a detailed surface model.

```
paintDamageIncrement =
      days * uvDamagePerIndexDay * uvIndex / paintDurability
    + days * saltDamagePerDay * saltExposure * (1 - washProtection)
    + distance * gritDamagePerKilometre * trafficDensity
```

Additional events:

- **Stone chips**: Poisson events with a rate proportional to highway distance and speed. Each chip adds a small damage increment.
- **Hail**: a hail day adds a large damage increment.
- **Parking dings**: Poisson events whose rate depends on parking situation: street, carport, or garage.
- **Rust**: when damage exceeds a threshold, wet and salty days grow a separate rust index. The output is the probability that rust has started by the horizon.

Washing and waxing reduce salt and ultraviolet damage for a period. The user's washing frequency is part of the case.

Paint repair is a small, optional cost. It does not feed back into vehicle value.

### 9.10 Windshield

```
chipCount += Poisson( windshieldChipRatePerHighwayKilometre * highwayDistance
                      * speedFactor * gravelFactor )
```

Each chip has a temperature-dependent probability of propagating into a crack on a cold day. A crack requires windshield replacement. Chips without a crack are reported but not repaired.

### 9.11 Consumables

Engine oil, oil filter, air filter, cabin filter, spark plugs, brake fluid, coolant, and wiper blades follow service intervals. They contribute cost and, in the case of oil and filters, affect the engine and emissions models. They are not reported as wear prognoses.

---

## 10. Fuel consumption

Fuel is computed per trip from a Willans-line formulation: wheel work plus engine friction plus accessories, divided by indicated efficiency, plus cold-start enrichment.

```
wheelEnergy =
      massVehicle * gravity * rollingResistanceCoefficient * distance
    + 0.5 * airDensity * dragArea * distance * meanSpeedMetresPerSecond^2
    + accelerationEnergy

rollingResistanceCoefficient =
    baseRollingResistance
    * (1 + wetRollingResistanceIncrease * indicator(wet))
    * (1 + pressureRollingResistanceSensitivity
           * max(0, referencePressure - pressure) / referencePressure)

meanEngineSpeed = idleEngineSpeed
                  + engineSpeedPerKilometrePerHour * meanSpeed

frictionPower = frictionMeanEffectivePressure * engineDisplacement
                * meanEngineSpeed / 120000

frictionEnergy = frictionPower * durationHours * 3.6

accessoryEnergy = (accessoryPower + airConditioningPower * indicator(airConditioning))
                  * durationHours * 3.6

fuelEnergy = (wheelEnergy / drivelineEfficiency + frictionEnergy + accessoryEnergy)
             / indicatedEfficiency

fuelLitres = fuelEnergy / gasolineEnergyDensity

coldStartExtraFuel = coldStartExtraFuelLitres
                     * (1 + coldStartFuelTemperatureSensitivity
                            * max(0, 20 - ambientTemperature) / 20)
                     * indicator(coldStart)

totalFuel = fuelLitres + coldStartExtraFuel
```

Nominal values: indicated efficiency 0.36, driveline efficiency 0.92, friction mean effective pressure 1.2 bar, accessory power 0.6 kilowatts, air-conditioning power 0.7 kilowatts, gasoline energy density 32 megajoules per litre.

The model is deliberately physical. It will not be tuned so that a particular car returns a particular figure. Appendix C shows a worked example and its order of magnitude.

---

## 11. Maintenance and replacement

### 11.1 Scheduled maintenance

Scheduled items are triggered by distance, time, or both:

| Item | Nominal interval |
|---|---|
| Engine oil and oil filter | 15,000 km or 12 months |
| Inspection | 12 months |
| Brake fluid | 24 months |
| Coolant | 60 months |
| Spark plugs | 60,000 km |
| Air filter | 30,000 km |
| Cabin filter | 20,000 km |
| Wiper blades | 12 months |
| Tire rotation | 10,000 km |
| Seasonal tire swap | Configured months |

### 11.2 Adherence and deferral

The user's maintenance adherence, zero to one, controls three things:

1. **Deferral.** A low-adherence owner delays service by up to the configured maximum deferral. Deferral raises oil degradation, wear, and hazard.
2. **Detection.** At inspection, a worn part is detected with a probability that increases with adherence. Undetected wear continues until it becomes a failure or until the next inspection.
3. **Replacement margin.** A careful owner replaces pads and tires before the hard limit; a careless owner runs them closer to the limit.

```
replacementTrigger = wearLimit + adherence * replacementMargin
```

### 11.3 Replacement events

A replacement is triggered when a measured thickness reaches its trigger, or when a service schedule requires it. Replacement resets the installed state to a new-part value, records the event and its cost, and leaves cumulative consumed life unchanged.

Parts replaced at thresholds: front and rear pads, front and rear discs, tires, battery on failure, suspension at inspection, clutch at inspection, exhaust and catalyst on inspection or failure, cooling components on failure, windshield on crack, paint on optional repair.

### 11.4 Parts quality

The user's chosen part quality, zero to one, affects installed cost and, for wear parts, the wear rate of the replacement:

```
installedCost = baseInstalledCost * (0.75 + 0.5 * partQuality) * regionalPriceIndex
replacementWearFactor = 1.15 - 0.30 * partQuality
```

A cheap part is cheaper today and wears faster. This is a real trade-off and belongs in the model, not in a footnote.

---

## 12. Failure model

### 12.1 Hazard form

For each part with a failure mode, the daily hazard is

```
hazardDaily = reliabilityPriorMultiplier
              * frailty
              * baseAnnualFailureRate
              * (1 + wearHazardMultiplier * wearIndex^wearHazardExponent)
              / 365

probabilityOfFailure = 1 - exp(-hazardDaily)
```

The wear index is the normalized 0-to-1 wear state, except for the engine, where it is equivalent kilometres divided by design life.

A failure event draws an installed repair cost from a log-normal distribution around the configured base cost, records downtime, and resets the part state. Failures are competing risks, not independent per-part coin flips: each part can fail at most once per run in the simplest configuration, with the option to allow multiple failures for cheap components.

### 12.2 Failure modes in scope

| Part | Failure modes | Modelled |
|---|---|---|
| Engine | Rings and bore, bearings, turbocharger, seals, ignition, timing | Yes, competing risks |
| Battery | Cold-cranking failure | Yes |
| Suspension | Dampers, bushings, joints | Yes, lumped |
| Clutch and transmission | Slip, judder | Yes, lumped |
| Exhaust | Corrosion perforation | Yes |
| Catalyst | Efficiency loss, poisoning | Yes, via emissions inspection |
| Cooling | Pump, thermostat, hoses | Yes, lumped |
| Windshield | Chip propagation to crack | Yes |
| Brakes | — | **No** |
| Paint | — | No failure; rust indicator only |

### 12.3 Consequences

Each failure contributes:

- the repair cost,
- a towing cost,
- downtime days valued at the configured daily rate,
- a reduction in the relevant wear index after repair.

There is no accident model and therefore no injury, liability, or body-damage cascade.

---

## 13. Monetary model

### 13.1 TCO definition

```
totalCostOfOwnership =
      fuelCost
    + scheduledMaintenanceCost
    + wearReplacementCost
    + unplannedRepairCost
    + consequentialCost
```

- **Fuel cost** = litres consumed multiplied by the fuel price.
- **Scheduled maintenance** = oil, filters, fluids, inspection, wipers, spark plugs, rotations, tire swaps.
- **Wear replacement** = pads, discs, tires, and any part replaced at a threshold or inspection.
- **Unplanned repair** = failure events and their parts.
- **Consequential** = towing plus downtime days multiplied by the daily value.

Depreciation is reported separately:

```
depreciation = distance * depreciationPerKilometre
               + horizonYears * depreciationPerYear
```

It is excluded from TCO because it is not a cash outflow.

Planned and unplanned costs are kept separate on purpose. If they were merged, a car that is simply due for pads and tires would look as expensive as a car that suffered a breakdown, and the model would effectively reward neglect. The report always shows both.

### 13.2 Reporting

A single run reports one cost figure. The harness reports TCO as a distribution across runs:

- mean,
- median,
- 10th and 90th percentiles,
- 95th percentile,
- 95% tail average, the mean of the worst 5% of outcomes,
- probability of exceeding a user-defined budget.

The breakdown is reported by category and by month. The monthly timeline answers the question "when does the money leave the account", which a single annual total cannot.

---

## 14. Output contract

The output contract has two levels. The **model** produces one run for one car. The **Monte Carlo harness** described in Section 15 consumes many runs and produces an aggregate. The two levels have separate documents.

### 14.1 Model output: one run

A run is a single simulation of one car over one horizon with one run seed. It is serialized as one TOML document. A run contains one outcome, not a distribution.

```toml
schema_version = "1.0"

[run]
case_name = "2019-golf-1.5-tsi"
start_date = 2026-10-01
horizon_days = 365
run_seed = 918273645

[run.totals]
distance_kilometres = 12430.0
fuel_litres = 806.2
fuel_consumption_litres_per_100_kilometres = 6.49
total_cost_of_ownership_euros = 2087.4
depreciation_euros = 1345.8

[run.cost_breakdown]
fuel_euros = 1410.9
scheduled_maintenance_euros = 200.0
wear_replacement_euros = 265.0
unplanned_repair_euros = 161.5
consequential_euros = 50.0

[run.final_state]
engine_wear_equivalent_kilometres = 9720.0
oil_degradation = 0.98
oil_level_fraction = 0.93
front_pad_millimetres = 8.1
rear_pad_millimetres = 6.9
front_disc_millimetres = 24.2
rear_disc_millimetres = 8.8
tire_tread_millimetres = [4.1, 4.1, 3.7, 3.7]
battery_state_of_health = 0.83
suspension_wear_index = 0.24
clutch_wear_index = 0.18
exhaust_corrosion_index = 0.29
catalyst_degradation_index = 0.21
paint_damage_index = 0.34
windshield_chip_count = 1
rust_started = false

[[run.events]]
day = 187
kind = "scheduled_maintenance"
item = "engine_oil_and_filter"
cost_euros = 110.0

[[run.events]]
day = 244
kind = "failure"
item = "battery"
cost_euros = 161.5

[[run.monthly]]
month = 1
cost_euros = 178.4
front_pad_millimetres = 8.8
tire_tread_millimetres = 4.9
```

A single run has no percentiles, no means, and no probabilities. Those are properties of a set of runs, not of one run. Optional content is omitted rather than encoded with a sentinel: an event that did not occur is simply absent from `[run.events]`.

### 14.2 Model output: run tables

A run also produces CSV tables for detailed analysis:

- `run_events.csv`, one row per event: day, kind, item, cost.
- `run_monthly.csv`, one row per month: cost and the principal wear states.
- `run_trips.csv`, optional, one row per trip, when trip-level analysis is requested.

### 14.3 Harness output: aggregate distribution

The Monte Carlo harness aggregates many runs. It writes one aggregate TOML document and one CSV row per run.

```toml
[aggregate.distance_kilometres]
mean = 12450.0
p10 = 10800.0
p50 = 12400.0
p90 = 14100.0

[aggregate.total_cost_of_ownership_euros]
mean = 2140.0
p10 = 1880.0
p50 = 2120.0
p90 = 2410.0
p95 = 2580.0
tail_average_95 = 2720.0

[[part_prognoses]]
id = "front_brake_pads"
label = "Front brake pads"
unit = "millimetres"
replaceable = true
failure_modelled = false
replacement_probability = 0.12
failure_probability = 0.0
expected_cost_euros = 11.0

[part_prognoses.state_at_horizon]
p10 = 4.8
p50 = 6.1
p90 = 7.4

[[failures]]
id = "battery"
probability_at_least_one = 0.04
expected_count = 0.04
```

Percentile tables contain at least `mean`, `p10`, `p50`, and `p90`. Cost tables additionally contain `p95` and `tail_average_95`. The per-run table `runs.csv` contains one row per run with the scalar outputs. Its columns include:

```
run_index, run_seed,
distance_kilometres, fuel_litres, fuel_consumption_litres_per_100_kilometres,
engine_wear_equivalent_kilometres, oil_degradation, oil_level_fraction,
front_pad_millimetres, rear_pad_millimetres, front_disc_millimetres, rear_disc_millimetres,
tire_tread_front_left_millimetres, tire_tread_front_right_millimetres,
tire_tread_rear_left_millimetres, tire_tread_rear_right_millimetres,
battery_state_of_health, suspension_wear_index, clutch_wear_index,
exhaust_corrosion_index, catalyst_degradation_index, paint_damage_index,
windshield_chip_count, rust_started,
fuel_cost_euros, scheduled_maintenance_euros, wear_replacement_euros,
unplanned_repair_euros, consequential_euros, total_cost_of_ownership_euros,
failure_count, downtime_days, depreciation_euros
```

### 14.4 Determinism

A run is reproducible from its resolved case and its run seed. The harness is reproducible from the case with its uncertainties, the master seed, and the number of runs. Floating-point formatting is fixed, and every seed is part of the output.

---

## 15. Monte Carlo harness

The harness is not part of the model. It is a consumer of the model that turns many runs into a predictive distribution. For each run it resolves the uncertain inputs of the case into concrete values, assigns a run seed, calls the model once, and collects the run result. After the last run it aggregates.

### 15.1 Number of runs

The default is 20,000 runs. A one-year run is cheap: roughly one thousand trips, so twenty thousand runs is on the order of twenty million trip updates, which runs in seconds on a modern desktop.

For stable estimates of rare events and tail costs, 100,000 runs are recommended. A failure probability of 5% then produces about 5,000 events, giving a relative standard error of roughly 1.4%.

### 15.2 Statistics

- Percentiles are computed with linear interpolation between order statistics.
- The 95% tail average is the mean of the largest 5% of outcomes.
- Correlations between principal outputs are reported because they matter: a car with high fuel consumption also tends to have high tire and brake wear, since all three share driving style.
- The variance decomposition of Section 6.1 is reported when epistemic sampling is enabled.

### 15.3 Common random numbers

Scenario comparison reuses the run seeds, so the same weather and trip streams are used across scenarios. If the user asks "what if this owner drove 10% less", the difference is not contaminated by a different weather year. This is a variance-reduction technique and a usability feature.

### 15.4 Sensitivity

The aggregate includes a diagnostic ranking: which inputs, if measured or changed, would most reduce the uncertainty of total cost of ownership. The ranking is computed from the correlation between sampled inputs and the outcome, not from a separate global sensitivity analysis in version 0.1. A full Sobol analysis is a roadmap item.

---

## 16. Parameterization

### 16.1 The configuration file

The configuration file is the single source of truth. It is a TOML document. Keys are snake_case, dates use the native TOML date type, lists use TOML arrays, and grouped settings use TOML tables.

| Section | Contents |
|---|---|
| `[case]` | Name, start date, horizon |
| `[run]` | Run seed for a single run |
| `[monte_carlo]` | Number of runs, master seed, epistemic sampling toggle (harness only) |
| `[output]` | Output directory and CSV toggles |
| `[vehicle]` | Mass, drag area, engine, transmission, drivetrain, component specifications |
| `[current_state]` | Measured condition; `[current_state.uncertainty]` holds the standard deviations |
| `[user]` | Commuting, errands, long trips, style, maintenance adherence, parking |
| `[climate]` | Zone preset plus overrides |
| `[maintenance]` | Intervals, deferral, detection, service costs |
| `[reliability]` | Failure rates, frailty, hazard multipliers |
| `[model]` | Physical and engineering constants |
| `[costs]` | Installed part and service costs |
| `[economics]` | Fuel price, price index, towing, downtime, depreciation, budget |
| `[route_classes]` | Speed, stop density, and acceleration per route class |

A minimal configuration looks like this:

```toml
schema_version = "1.0"

[case]
name = "2019-golf-1.5-tsi"
start_date = 2026-10-01
horizon_days = 365

[run]
run_seed = 918273645

[monte_carlo]
number_of_runs = 20000
master_seed = 20261021
sample_epistemic_uncertainty = true

[output]
directory = "output"
write_run_tables = true
write_runs_csv = true

[vehicle]
mass_kilograms = 1450.0
drag_area_square_metres = 0.68
engine_displacement_litres = 1.5
turbocharged = true
transmission = "manual"
drivetrain = "front"
odometer_kilometres = 85000.0
vehicle_age_years = 6.0

[user]
commute_distance_kilometres = 25.0
commute_days_per_week = 5
style_aggressiveness = 0.4
maintenance_adherence = 0.7
parking = "street"

[climate]
zone = "temperate-maritime"

[current_state]
front_pad_thickness_millimetres = 9.0
rear_pad_thickness_millimetres = 7.5
tire_tread_millimetres = [5.0, 5.0, 4.6, 4.6]

[current_state.uncertainty]
front_pad_thickness_millimetres = 1.5
tire_tread_millimetres = 0.5
```

Every field has a documented default. A minimal configuration may specify only the case, the vehicle, and the user; the remainder falls back to defaults. The output echoes the fully resolved configuration.

The TOML parser is `toml++`, a header-only library and the only third-party dependency besides Google Test. Parse errors report the file, line, and column. Unknown keys inside a known table produce a warning rather than being silently dropped; an unknown schema major version is an error.

### 16.2 Schema versioning

The configuration and output documents carry a schema version. A breaking change increments the major version. The engine rejects a configuration with an unknown major version rather than silently ignoring fields.

### 16.3 Validation of inputs

Before simulation, the engine checks:

- positive mass, displacement, and horizon,
- thickness values within physical bounds,
- probabilities within zero and one,
- non-negative costs and distances,
- a start date that parses,
- a climate zone that exists or a complete override,
- a positive number of runs for the harness and a non-empty horizon for the model.

Invalid input produces a clear error with the offending key and value, not a silent fallback.

### 16.4 Nominal constants and ranges

Nominal values are listed in Appendix B with their provenance class. They are priors. Where a range is known, it is documented; where it is not, the value is marked as an engineering estimate and should be treated as the weakest part of the model.

---

## 17. Validation

Validation for a single case cannot rely on a ground-truth outcome. It relies on structure, invariants, and external priors.

### 17.1 Structural tests

- **Accounting identity**: the sum of cost components equals TCO.
- **Conservation**: cumulative distance equals the sum of trip distances.
- **State bounds**: every health index stays within its admissible range.
- **Reset consistency**: a replacement resets the installed state and leaves cumulative life unchanged.
- **Monotonicity**: more aggressive driving increases every wear output; more highway distance decreases brake wear per kilometre; colder weather increases cold-start wear and battery failure probability; more salt increases corrosion and paint damage.
- **Weather monotonicity**: a colder year produces more engine wear and more battery failures than a warmer year with the same trips.
- **Determinism**: the same resolved case and run seed produce identical output.
- **Model purity**: a run takes a resolved case and a run seed, resolves no input uncertainty, and returns no aggregate statistics.

### 17.2 Sanity bounds

These are pass/fail bounds, not targets:

| Quantity | Plausible range |
|---|---|
| Fuel consumption, mixed driving | 4 to 15 litres per 100 km |
| Front pad life | 25,000 to 90,000 km |
| Tire tread life | 25,000 to 70,000 km |
| Engine wear increment per year | 3% to 15% of design life |
| Annual failure probability, whole car | 2% to 30% |

A result outside these bounds indicates a modeling or input error. A result inside them is not evidence of accuracy.

### 17.3 External validation

Real validation requires data that version 0.1 does not have:

- service records with pad, disc, and tire replacements,
- fuel logs,
- battery replacement dates,
- failure and repair records,
- measured tread and pad depths over time.

The calibration plan is to fit only the coefficients that are identifiable from such data, and to leave structural choices untouched. Until then, every output carries a model-version and prior-provenance note.

The relative importance of battery, brake, suspension, and age-dependent defects is anchored to the ADAC and TÜV reports in Appendix F. Those reports are sanity anchors and sources for relative priors, not calibration data for absolute rates.

---

## 18. Limitations

1. **No trajectory simulation.** Gear selection, transient enrichment, thermal transients, and road grade are not modelled. Drives are summarized by sufficient statistics.
2. **No brake failure.** By scope decision, brakes wear but do not fail.
3. **No accidents.** There is no crash, injury, liability, or body-damage cascade.
4. **Paint is coarse.** It is a condition index with event jumps, not a surface-damage model, and it has little monetary weight.
5. **Engine wear is latent.** It cannot be measured directly. The model output is a model-derived index, not an observation. Oil analysis would be the only external proxy.
6. **Failure priors are weak.** Annual failure rates for specific parts of specific vehicles are not available in this project. They are engineering priors and should be read as such.
7. **Synthetic weather.** The climate process reproduces seasonality and persistence, not a specific historical year.
8. **No behavioral drift.** The user's driving and maintenance behavior are fixed for the horizon.
9. **Single vehicle.** There is no fleet, no shared-parts correlation, and no second car.
10. **No market model.** Depreciation is a simple linear estimate; resale value is not predicted.

These limitations are deliberate. Each one is a candidate for a later version, not an oversight.

---

## 19. Roadmap

| Version | Content |
|---|---|
| 0.1 | This model: single case, gasoline, one year, TOML and CSV output |
| 0.2 | Historical weather replay from a CSV; calibration harness |
| 0.3 | Global sensitivity analysis; uncertainty decomposition in the harness aggregate |
| 0.4 | Additional components and regional inspection regimes |
| 0.5 | Hybrid and electric powertrains |
| Later | Accident cascade, brake failure, market value model |

---

## Appendix A — Notation

| Name | Meaning | Unit |
|---|---|---|
| `distance` | Trip distance | kilometres |
| `meanSpeed` | Average speed including stops | kilometres per hour |
| `stopDensity` | Stops per kilometre | per kilometre |
| `brakeOnsetSpeed` | Speed at which braking begins | metres per second |
| `brakeEnergy` | Energy dissipated by friction brakes in a trip | megajoules |
| `accelerationEnergy` | Energy supplied for positive acceleration | megajoules |
| `style` | Driver aggressiveness | zero to one |
| `engineWearIndex` | Engine wear divided by design life | zero to one |
| `oilDegradation` | Oil life consumed | zero to one |
| `stateOfHealth` | Battery health | zero to one |
| `wearIndex` | Generic normalized wear | zero to one |
| `hazardDaily` | Daily failure hazard | per day |
| `frailty` | Car-level reliability multiplier | dimensionless |
| `totalCostOfOwnership` | Cash operating cost over the horizon | euros |

---

## Appendix B — Model constants

All values are defaults and all are overridable. Provenance classes: physical, specification, engineering estimate, literature prior.

### B.1 Engine

| Constant | Nominal value | Unit | Provenance |
|---|---|---|---|
| `coldStartWearBaseKilometres` | 5 | equivalent km | engineering estimate |
| `coldStartWearTemperatureScale` | 20 | degrees Celsius | engineering estimate |
| `coldStartWearMaximum` | 20 | equivalent km | engineering estimate |
| `coldStartThreshold` | 120 | minutes | engineering estimate |
| `shortTripDistance` | 5 | kilometres | engineering estimate |
| `engineDesignLife` | 250,000 | kilometres | engineering estimate |
| `oilDegradationPerKilometre` | 1 / 15,000 | per kilometre | specification |
| `oilDegradationPerColdStart` | 1 / 900 | per event | engineering estimate |
| `oilDegradationPerDay` | 1 / 900 | per day | engineering estimate |
| `oilWearSensitivity` | 1.5 | dimensionless | engineering estimate |
| `oilWearExponent` | 2 | dimensionless | engineering estimate |
| `oilLevelCriticalFraction` | 0.25 | fraction | specification |
| `oilLevelStarvationWearFactor` | 2 | dimensionless | engineering estimate |
| `oilStarvationHazardMultiplier` | 30 | dimensionless | engineering estimate |
| `turboWearPerCycle` | 0.00001 | per event | engineering estimate |
| `turboHotShutdownWear` | 0.01 | per event | engineering estimate |

### B.2 Brakes

| Constant | Nominal value | Unit | Provenance |
|---|---|---|---|
| `frontBrakeEnergyShare` | 0.72 | fraction | engineering estimate |
| `padWearMillimetresPerMegajoule` | 0.010 | mm per MJ | engineering estimate |
| `discWearRatio` | 0.15 | mm per mm | engineering estimate |
| `brakeTemperatureRisePerMegajoulePerHour` | 8 | degrees Celsius | engineering estimate |
| `brakeTemperatureWearStart` | 250 | degrees Celsius | engineering estimate |
| `brakeTemperatureWearFactor` | 2 | dimensionless | engineering estimate |
| `brakeCorrosionPerSaltDay` | 0.003 | mm per day | engineering estimate |
| `brakeFluidLife` | 24 | months | specification |

### B.3 Tires

| Constant | Nominal value | Unit | Provenance |
|---|---|---|---|
| `tireWearPerKilometre` | 0.00014 | mm per km | engineering estimate |
| `tireLoadWearExponent` | 1.0 | dimensionless | engineering estimate |
| `tirePressureWearExponent` | 0.5 | dimensionless | engineering estimate |
| `tireLateralWearSensitivity` | 0.8 | dimensionless | engineering estimate |
| `tireTemperatureWearSensitivity` | 0.25 | dimensionless | engineering estimate |
| `tirePressureLeakPerMonth` | 0.05 | bar per month | engineering estimate |
| `tireAgeLimit` | 6 | years | specification |
| `winterTireWearFactor` | 1.2 | dimensionless | engineering estimate |
| `referenceMass` | 1,450 | kilograms | specification |
| `referencePressure` | 2.3 | bar | specification |
| `wetSurfaceWearIncrease` | 0.05 | dimensionless | engineering estimate |
| `gravelSurfaceWearIncrease` | 0.30 | dimensionless | engineering estimate |

### B.4 Battery

| Constant | Nominal value | Unit | Provenance |
|---|---|---|---|
| `batteryAgeFactor` | 0.04 | per square-root year | literature prior |
| `batteryHeatDamage` | 0.00001 | per degree-day above 25 | literature prior |
| `batteryCycleDamage` | 0.000001 | per start | literature prior |
| `batteryRequiredHealthBase` | 0.30 | dimensionless | engineering estimate |
| `batteryRequiredHealthPerDegree` | 0.005 | per degree below 20 | engineering estimate |
| `batteryFailureSteepness` | 25 | dimensionless | engineering estimate |

### B.5 Other components

| Constant | Nominal value | Unit | Provenance |
|---|---|---|---|
| `suspensionWearPerKilometre` | 1 / 120,000 | per kilometre | engineering estimate |
| `clutchWearPerStartManual` | 1 / 150,000 | per start | engineering estimate |
| `clutchWearPerStartAutomatic` | 1 / 600,000 | per start | engineering estimate |
| `exhaustCorrosionPerDay` | 1 / 3,650 | per day | engineering estimate |
| `shortTripCorrosionFactor` | 3 | dimensionless | engineering estimate |
| `exhaustSaltCorrosionPerDay` | 1 / 1,200 | per day | engineering estimate |
| `catalystDegradationPerKilometre` | 1 / 200,000 | per kilometre | engineering estimate |
| `coolingWearPerDay` | 1 / 3,650 | per day | engineering estimate |
| `coolingWearPerThermalCycle` | 1 / 5,000 | per trip | engineering estimate |

### B.6 Paint and windshield

| Constant | Nominal value | Unit | Provenance |
|---|---|---|---|
| `uvDamagePerIndexDay` | 1 / 15,000 | per day | engineering estimate |
| `saltDamagePerDay` | 1 / 6,000 | per day | engineering estimate |
| `gritDamagePerKilometre` | 1 / 400,000 | per kilometre | engineering estimate |
| `stoneChipRatePerHighwayKilometre` | 0.002 | per kilometre | engineering estimate |
| `stoneChipDamage` | 0.004 | index | engineering estimate |
| `hailDamage` | 0.15 | index | engineering estimate |
| `parkingDingRatePerDay` | 0.0002 | per day | engineering estimate |
| `washProtectionPerWashPerMonth` | 0.05 | fraction | engineering estimate |
| `rustThreshold` | 0.55 | index | engineering estimate |
| `windshieldChipRatePerHighwayKilometre` | 0.0004 | per kilometre | engineering estimate |
| `windshieldCrackProbabilityPerChipPerColdDay` | 0.02 | per event | engineering estimate |

### B.7 Fuel and reliability

| Constant | Nominal value | Unit | Provenance |
|---|---|---|---|
| `indicatedEfficiency` | 0.36 | fraction | engineering estimate |
| `drivelineEfficiency` | 0.92 | fraction | engineering estimate |
| `frictionMeanEffectivePressure` | 1.2 | bar | engineering estimate |
| `accessoryPower` | 0.6 | kilowatts | specification |
| `airConditioningPower` | 0.7 | kilowatts | specification |
| `gasolineEnergyDensity` | 32 | MJ per litre | physical |
| `coldStartExtraFuel` | 0.22 | litres | engineering estimate |
| `coldStartFuelTemperatureSensitivity` | 0.05 | per degree Celsius | engineering estimate |
| `airDensity` | 1.2 | kilograms per cubic metre | physical |
| `gravity` | 9.81 | metres per second squared | physical |
| `engineSpeedPerKilometrePerHour` | 18 | revolutions per minute per km/h | engineering estimate |
| `wetRollingResistanceIncrease` | 0.15 | dimensionless | engineering estimate |
| `pressureRollingResistanceSensitivity` | 0.30 | dimensionless | engineering estimate |
| `engineAnnualFailureRate` | 0.008 | per year | literature prior |
| `engineWearHazardMultiplier` | 15 | dimensionless | engineering estimate |
| `wearHazardExponent` | 3 | dimensionless | engineering estimate |
| `suspensionAnnualFailureRate` | 0.02 | per year | literature prior |
| `clutchAnnualFailureRate` | 0.01 | per year | literature prior |
| `exhaustAnnualFailureRate` | 0.015 | per year | literature prior |
| `coolingAnnualFailureRate` | 0.015 | per year | literature prior |
| `frailtySigma` | 0.45 | dimensionless | engineering estimate |

---

## Appendix C — Worked example

This example is an order-of-magnitude check, not a calibration target.

Inputs: 25 km commute, mean speed 45 km/h, 0.35 stops per kilometre, vehicle mass 1,450 kg, drag area 0.68 square metres, rolling resistance 0.010, 20 degrees Celsius, dry road, 1.5 litre engine, friction mean effective pressure 1.2 bar, idle speed 800 rpm, engine speed slope 18 rpm per km/h, indicated efficiency 0.36, driveline efficiency 0.92, accessory power 0.6 kilowatts, gasoline 32 megajoules per litre.

| Quantity | Calculation | Result |
|---|---|---|
| Duration | 25 / 45 | 0.556 h |
| Stops | 0.35 × 25 | 8.75 |
| Brake onset speed | 0.7 × 12.5 | 8.75 m/s |
| Brake energy | 0.5 × 1450 × 8.75² × 8.75 | 0.486 MJ |
| Rolling energy | 1450 × 9.81 × 0.010 × 25,000 | 3.56 MJ |
| Aerodynamic energy | 0.5 × 1.2 × 0.68 × 12.5² × 25,000 | 1.59 MJ |
| Acceleration energy | 0.5 × 1450 × 10² × 8.75 | 0.63 MJ |
| Mean engine speed | 800 + 18 × 45 | 1,610 rpm |
| Friction power | 120 × 1.5 × 1610 / 120,000 | 2.42 kW |
| Friction energy | 2.42 × 0.556 × 3.6 | 4.83 MJ |
| Accessory energy | 0.6 × 0.556 × 3.6 | 1.20 MJ |
| Wheel energy | 3.56 + 1.59 + 0.63 | 5.78 MJ |
| Fuel energy | (5.78 / 0.92 + 4.83 + 1.20) / 0.36 | 34.2 MJ |
| Fuel volume | 34.2 / 32 | 1.07 L |
| Consumption | 1.07 / 25 × 100 | 4.3 L/100 km |

Adding a cold-start enrichment of about 0.22 litres and summer air-conditioning of about 0.04 litres raises the trip to roughly 1.33 litres, or 5.3 litres per 100 kilometres. Real mixed driving is usually higher because of transients, idling in traffic, and accessory loads; the model is intentionally physical and its constants are priors.

Brake check: front share 0.72 × 0.486 MJ = 0.350 MJ, multiplied by 0.010 mm per MJ gives 0.0035 millimetres per trip. Two hundred fifty commute trips per year give about 0.9 millimetres per year, or roughly 50,000 to 70,000 kilometres to the wear limit. This is the right order of magnitude.

Tire check: 25 km × 250 trips = 6,250 km per year at 0.00014 mm per km gives 0.9 millimetres per year, or about 45,000 kilometres to the 1.6-millimetre limit. This is also the right order of magnitude.

Engine check: 6,250 km plus 250 cold starts at 5 equivalent kilometres each gives 7,500 equivalent kilometres, or 3% of a 250,000-kilometre design life per year. At -10 degrees Celsius the cold-start equivalent rises to about 20 kilometres and the annual total to roughly 11,000 equivalent kilometres, or 4.5% of design life.

---

## Appendix D — Event ordering

The model, for one run:

```
run( resolvedCase, runSeed ):
    for each day from 1 to horizon:
        weather = generateWeather(day)
        if seasonalTireSwapScheduled(day): swapTireSet()
        trips = generateTrips(day, weather)

        for each trip:
            fuel = computeFuel(trip, state)
            updateEngine(trip, state)
            updateBrakes(trip, state)
            updateTires(trip, state)
            updateClutch(trip, state)
            updateSuspension(trip, state)
            updateExhaustAndCatalyst(trip, state)
            updatePaint(trip, state)
            updateWindshield(trip, state)

        applyDailyAging(state, weather)
        applyScheduledMaintenance(state, day)
        applyThresholdReplacements(state, day)
        sampleFailures(state, day, frailty)
        recordMonthlySnapshot(state, day)
    return the run result
```

The resolved case already carries the concrete current state and the frailty factor; the model does not draw them.

The harness, for many runs:

```
monteCarlo( case, numberOfRuns, masterSeed ):
    for runIndex in 1 .. numberOfRuns:
        resolvedCase = resolve( case, masterSeed, runIndex )   # samples current state, frailty, priors
        runSeed      = hash( masterSeed, runIndex, "run" )
        results[ runIndex ] = run( resolvedCase, runSeed )
    aggregate( results )
```

Rationale: driving happens before maintenance, maintenance happens before failures, and failures are resolved at the end of the day. This ordering is deterministic and testable. It is also an approximation: a failure that would occur mid-trip is assigned to the end of the day. For a one-year horizon this is immaterial for cost and wear, and it avoids a nested intra-trip event loop.

---

## Appendix E — Resolved design decisions

These were the open questions in the first draft. They are now closed for version 0.3.

| Topic | Decision | Rationale |
|---|---|---|
| Model boundary | The model simulates one car for one run and contains no Monte Carlo. A separate harness resolves uncertain inputs, assigns run seeds, calls the model, and aggregates the runs. | Keeps the model a pure, reproducible function of its inputs and lets sampling and statistics evolve independently of the physics. |
| Route classes | Keep four configurable classes: city, mixed, rural, highway. No separate average-speed input. | Distance, class, style, and weather already determine the speed distribution. An additional speed input would be redundant and hard to interpret. |
| Seasonal tires | Default on for continental, alpine, and Nordic zones; default off for maritime, Mediterranean, and arid zones. Always overridable. | Matches common practice without forcing seasonal tires on every case. |
| Paint repair cost | Reported as a separate line, excluded from TCO by default through `include_paint_repair_in_tco = false`. | Keeps TCO comparable across cases and reflects the low value impact of cosmetic paint damage. |
| Default economics | Euro, Germany, 2026 price level. Fuel 1.75 euro per litre, regional price index 1.0. | A single explicit baseline is easier to audit than a currency-neutral model. The configuration can override every value. |
| Failure priors | Relative weights anchored to the ADAC and TÜV reports in Appendix F. Absolute annual rates remain literature and engineering priors. | Avoids false precision while keeping the failure model plausible and documented. |
| Horizon | Any horizon from one day upward. Default 365 days. | The simulation loop is horizon-agnostic; multi-year runs cost only compute time. |
| Output format | Run output and harness aggregate as TOML; CSV tables for per-run and per-month data. No JSON. | TOML is readable and has a mature header-only parser; CSV remains the right format for large run tables. |

The decisions do not remove the limitations listed in Section 18. In particular, absolute failure rates remain priors until service and repair data are available.

---

## Appendix F — Sources and calibration anchors

The model is not calibrated to these sources. They inform relative priors and sanity anchors, not absolute predictions.

1. **ADAC Pannenstatistik 2026**, "Die Gewinner und Verlierer", 23 April 2026.
   <https://www.adac.de/rund-ums-fahrzeug/unfall-schaden-panne/adac-pannenstatistik-2026/>
   Accessed 2026-10-22.
   Used for: the starter battery is the most common breakdown cause, with a share of roughly 44 to 45 percent in the 2026 statistics. This supports the weight given to battery health and cold-cranking failure.

2. **TÜV-Verband, TÜV-Report 2026**, "Mängelquoten steigen zum vierten Mal in Folge", 20 November 2025.
   <https://www.tuev-verband.de/pressemitteilungen/tuev-report-2026>
   Accessed 2026-10-22.
   Used for: 21.5 percent of inspected cars had significant or dangerous defects, 12.3 percent had minor defects, and 66.1 percent were defect-free. The average German car was 10.6 years old. Defect rates rise with vehicle age, and brakes, springs, and dampers are among the most frequent defects. This supports the age-dependent hazard structure and the inclusion of suspension and brakes in the wear model.

3. **toml++ 3.4.0**, TOML parser and serializer.
   <https://github.com/marzer/tomlplusplus>
   Implementation dependency, not a model source.

4. **Google Test 1.18.0**, test framework.
   <https://github.com/google/googletest>
   Implementation dependency, not a model source.

---

*End of document.*
