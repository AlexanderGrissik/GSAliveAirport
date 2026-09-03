# Small-Aircraft Ground Object Options

This document lists the installed ground-object options relevant to the small aircraft category. FSDT liveries and closely related variants are collapsed using `*`.

The wildcard is documentation shorthand only. SimConnect requires a concrete SimObject title when an object is created.

## 1. Pushback candidates

### Stock Asobo/Microsoft

```text
ASO_Aircraft_Caddy
ASO_Pushback_Blue
ASO_Pushback_White
Microsoft_Pushback_02*
Microsoft_Pushback_03*
Pushback 02
Pushback 03
```

`ASO_Aircraft_Caddy` is explicitly listed by the SDK under `<SmallPushbacks>`, making it the best stock option for the small category.

Exact Microsoft variants covered above:

```text
Microsoft_Pushback_02
Microsoft_Pushback_02_blue
Microsoft_Pushback_02_green
Microsoft_Pushback_02_orange

Microsoft_Pushback_03
Microsoft_Pushback_03_Blue
Microsoft_Pushback_03_Green
Microsoft_Pushback_03_Orange
```

The following are baggage tractors, not pushback vehicles:

```text
ASO_Tug01_*
ASO_Tug02_*
```

### All installed FSDT pushback families

```text
FSDT_Pushback_M1A*
FSDT_Pushback_Mototok_8600MA*
FSDT_TPX_100_E*
FSDT_Pushback_TMX_150*
FSDT_TPX_200*
FSDT_Pushback_Trepel_280*
FSDT_Pushback_03*
FSDT_Pushback_03_ASOBO*
FSDT_Pushback_Mototok_Spacer200*
FSDT_TPX_500_MTS*
FSDT_Pushback_Trepel_700*
```

Their GSX suitability ranges are:

| Family | GSX selection |
|---|---|
| `FSDT_Pushback_M1A*` | Up to 50,000 kg |
| `FSDT_Pushback_Mototok_8600MA*` | Up to 176,000 kg, towbarless |
| `FSDT_TPX_100_E*` | 20,000-150,000 kg |
| `FSDT_Pushback_TMX_150*` | 50,000-120,000 kg |
| `FSDT_TPX_200*` | 50,000-200,000 kg |
| `FSDT_Pushback_Trepel_280*` | 120,000-240,000 kg |
| `FSDT_Pushback_03*` | At least 190,000 kg |
| `FSDT_Pushback_Mototok_Spacer200*` | At least 176,000 kg |
| `FSDT_TPX_500_MTS*` | At least 200,000 kg |
| `FSDT_Pushback_Trepel_700*` | At least 240,000 kg |

For the small wingspan category, the realistic selection pool should be:

```text
ASO_Aircraft_Caddy
FSDT_Pushback_M1A*
FSDT_Pushback_Mototok_8600MA*
FSDT_TPX_100_E*
```

`FSDT_Pushback_M1A*` is the best conventional small tug.

## 2. Walking workers

### Stock Asobo Tarmac workers

All combinations of gender, season, and ethnicity exist:

```text
Tarmac_Female_Summer_*
Tarmac_Female_Winter_*
Tarmac_Male_Summer_*
Tarmac_Male_Winter_*
```

Each `*` can be:

```text
African
Arab
Asian
Caucasian
Hispanic
Indian
```

This produces 24 stock Tarmac workers.

### Stock Asobo marshallers

```text
Marshaller_Female_Summer_*
Marshaller_Female_Winter_*
Marshaller_Male_Summer_*
Marshaller_Male_Winter_*
```

Again, each wildcard can be:

```text
African
Arab
Asian
Caucasian
Hispanic
Indian
```

This produces 24 stock marshallers.

These Asobo characters can walk when created as an `IdleWorker` through the mission/service animation system. When directly spawned using the current SimConnect method, the tested Asobo marshaller remained standing. They cannot currently use the FSDT frame technique.

### FSDT wing walkers

```text
FSDT_Wingwalker_Male_*
FSDT_Wingwalker_Female_*
```

Concrete titles:

```text
FSDT_Wingwalker_Male_01
FSDT_Wingwalker_Male_04
FSDT_Wingwalker_Male_Brian
FSDT_Wingwalker_Female_01
FSDT_Wingwalker_Female_06
```

### FSDT marshallers

```text
FSDT_Marshaller_0*
FSDT_Marshaller_Brenda_*
FSDT_Marshaller_Donald_*
FSDT_Marshaller_Emma_*
FSDT_Marshaller_Will_*
```

These cover all 17 installed FSDT marshallers.

### Other FSDT walking-worker families

```text
FSDT_MAN1_STAIR_OPERATOR
FSDT_Operator_Staircase_Aviramp
FSDT_Fueler_Man1_Mercedes
FSDT_MAN1_UPS

FSDT_cleaning_crew_F_*
FSDT_cleaning_crew_M_*
FSDT_cleaning_crew_SF_*
FSDT_cleaning_crew_SM_*

FSDT_Attendant_hs*
```

`FSDT_Attendant_hs*` covers 24 human models, attached variants, and thousands of airline liveries. They technically have walking animation but look like airline crew rather than ramp workers.

The following have activity or operation animations but no normal configurable walking loop:

```text
FSDT_catering_man_*
FSDT_Baggage_Loader_Man_*
FSDT_MAN1_Staircase_CDS
FSDT_WOMAN1_Staircase_CDS
```

For the small-aircraft worker pool, the initial candidates should be:

```text
FSDT_Wingwalker_Male_*
FSDT_Wingwalker_Female_*
FSDT_cleaning_crew_*
```

Only `FSDT_Wingwalker_Male_04` is currently runtime-verified.

## 3. Small passenger vans

### Confirmed FSDT passenger vehicles

```text
FSDT_Van_Passengers*
```

This covers the generic model and all 493 livery variants. Its configuration specifies:

```text
capacity = 5
condition = numPassengers <= 5
```

A slightly larger option is:

```text
FSDT_PassengerShuttle*
```

Its capacity is 17, and GSX selects it for 6-17 passengers.

### Stock Asobo/Microsoft passenger vehicles

```text
ASO_Shuttle_01*
Van Asia High Roof Passenger
Van Asia Low Roof Passenger
```

Concrete shuttle titles:

```text
ASO_Shuttle_01
ASO_Shuttle_01_Gray
ASO_Shuttle_01_Yellow
```

### Generic stock vans that could visually substitute

```text
Microsoft_Van_ASIA_02*
Microsoft_Van_EUR*
Microsoft_Van_NA_Modern*

Van Asia Vintage
Van Europe
Van NorthAm
```

These are spawnable small vans, but they are not explicitly configured as passenger-service vehicles.

Recommended pool:

```text
FSDT_Van_Passengers*
ASO_Shuttle_01*
Van Asia High Roof Passenger
Van Asia Low Roof Passenger
```

## 4. Standing passengers

There are 63 installed non-seated FSDT passenger models.

```text
FSDT_Passenger_BUSINESS_F_*
FSDT_Passenger_BUSINESS_M_*

FSDT_Passenger_CASUAL_F_*
FSDT_Passenger_CASUAL_M_*

FSDT_Passenger_Cboy*
FSDT_Passenger_CGirl_*
FSDT_Passenger_CMan_*
FSDT_Passenger_CWom_*

FSDT_Passenger_KID_F_*
FSDT_Passenger_KID_M_*

FSDT_Passenger_PARTY_F*
FSDT_Passenger_PARTY_M*

FSDT_Passenger_RP_*

FSDT_Passenger_PILOT_*
FSDT_Passenger_LOADMASTER_*
```

Counts:

| Pattern | Models |
|---|---:|
| `FSDT_Passenger_BUSINESS_F_*` | 2 |
| `FSDT_Passenger_BUSINESS_M_*` | 2 |
| `FSDT_Passenger_CASUAL_F_*` | 7 |
| `FSDT_Passenger_CASUAL_M_*` | 15 |
| `FSDT_Passenger_Cboy*` | 3 |
| `FSDT_Passenger_CGirl_*` | 2 |
| `FSDT_Passenger_CMan_*` | 3 |
| `FSDT_Passenger_CWom_*` | 1 |
| `FSDT_Passenger_KID_F_*` | 1 |
| `FSDT_Passenger_KID_M_*` | 1 |
| `FSDT_Passenger_PARTY_F*` | 1 |
| `FSDT_Passenger_PARTY_M*` | 1 |
| `FSDT_Passenger_RP_*` | 18 |
| `FSDT_Passenger_PILOT_*` | 5 |
| `FSDT_Passenger_LOADMASTER_*` | 1 |

For ordinary GA passengers, use:

```text
FSDT_Passenger_BUSINESS_F_*
FSDT_Passenger_BUSINESS_M_*
FSDT_Passenger_CASUAL_F_*
FSDT_Passenger_CASUAL_M_*
FSDT_Passenger_RP_*
```

Optionally include children:

```text
FSDT_Passenger_Cboy*
FSDT_Passenger_CGirl_*
FSDT_Passenger_KID_F_*
FSDT_Passenger_KID_M_*
```

Exclude:

```text
FSDT_Passenger_*_SEATED
```

Those are seated versions intended for vehicle or aircraft interiors.

No standalone Asobo passenger titles appeared in the spawnable catalog. The Asobo `Tarmac_*` people can be used as standing humans, but visually they are airport workers rather than passengers.

