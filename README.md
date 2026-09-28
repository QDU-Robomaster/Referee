# Referee

RM_Referee_2025

## Shared message types

`RefereeTypes.hpp` provides the producer-owned `RobotGameRefereePack` and its
component types without including CMD, UART or the parser. Host subscribers can
include this header directly. Existing `Referee::RobotGameRefereePack` and component
names are aliases to these same types; field order and packed wire layout are unchanged.

## Required Hardware

cmd

## Constructor Arguments

None

## Template Arguments

None

## Depends

None
