# Electroscan – A Random PCB Fault Detection System

## Project Deck
https://drive.google.com/file/d/1ErZxJuves5wOGGoEg1BYj4MtENemJXBk/view?usp=drivesdk

A doctor can't cure a disease he can't identify — a PCB isn't any different. Electroscan is built on that instinct: know the fault before you touch the board.
Manually probing a PCB with a multimeter to find opens, shorts, or bad components takes forever and it's easy to miss things. Electroscan automates that diagnosis — it scans a board's test points, measures resistance and voltage at each one, and flags anything that doesn't match what's expected.

## How it actually works (step by step)

1. **Upload the PCB image** — the user uploads a photo of the target PCB to the web platform.
2. **Mark the nodes** — on that image, the user manually marks all the visible connected nodes and labels the type of device between each pair (switch, LED, diode, resistor, capacitor, etc.) — no need to manually enter resistance values.
3. **Generate the expected matrix** — based on the device type marked at each node, the platform works out the expected resistance behavior and builds an expected resistance matrix for the board.
4. **Sync to the web page** — this expected matrix is pushed to a web page that acts as the bridge between the platform and the hardware.
5. **ESP32 pulls the data** — the ESP32 connects to that web page and downloads the expected resistance matrix.
6. **Measure the real board** — the ESP32 scans the physical PCB through the MUX/DMUX matrix and builds the actual (measured) resistance matrix.
7. **Compare** — measured values are checked against the expected matrix, node by node.
8. **Repeat for reliability** — each scan is run multiple times to rule out one-off noise or misreads before flagging something as a fault.
9. **Report the result** — the system outputs the total number of faults found, broken down into sub-categories (different types of shorts, opens, etc.) rather than just a pass/fail number.

## Key thing about it

Electroscan is not built for one specific board — it's designed to work with **random, arbitrary PCBs**. Most fault-detection setups are hardwired to test a particular circuit. Because the expected matrix comes from marking device types on a user-uploaded image rather than a fixed reference design, the same hardware can be pointed at pretty much any PCB, not just the one it was originally built for.

This is also *why* the image upload step exists in the first place. If the ESP32 measures an open circuit between two points, that alone doesn't tell you anything — it could be a genuine fault, or it could just be how that part of the board is designed to work (e.g. a switch that's meant to be open until pressed). Without knowing what's actually supposed to be there, there's no way to tell a real fault from expected behavior. Marking the devices on the image upfront gives the system a reference to compare against, so an "open" only gets flagged as a fault when it doesn't match what was supposed to be there.

## How it works

At the core, it's an ESP32 talking to a bank of CD4051/HCF4051 multiplexers and demultiplexers. Instead of wiring up a separate ADC channel for every single test point on the board (which gets messy fast), the MUX/DMUX setup lets me route each point through to one shared measurement channel one at a time. The ESP32 handles the switching logic and reads the ADC values as each point gets its turn.

## The hard parts

A few things took real trial and error to get right:

- **Keeping channels from stepping on each other** — since multiple CD4051s share the same select lines, I had to be careful about sequencing so one channel's reading didn't bleed into the next.
- **EN pin timing** — making sure only one channel was actually "live" at any given moment during a scan.
- **Getting clean ADC readings** — noise and settling time were a bigger issue than I expected; had to tune the timing to get consistent numbers on repeated scans.

## Proving it actually works

Rather than just simulating faults, I hand-etched a small NE555 timer PCB myself and used it as a real test board — some parts working, some deliberately faulty. Running Electroscan against this physical board was the real test of whether the fault detection logic held up outside of theory.

## Built with

- ESP32 (Arduino framework)
- CD4051 / HCF4051 multiplexer/demultiplexer ICs
- A home-etched NE555 timer PCB as the test board

## Where it's at

Still a prototype — this is a personal project I built as part of my intern, not a finished product.

## What I'd add next

- Auto-generated fault reports instead of raw readings
- Support for a bigger grid of test points
- A simple UI to visualize scan results instead of reading them off the serial monitor

  ## Project Image
  <img width="1421" height="897" alt="image" src="https://github.com/user-attachments/assets/c8b170fd-8cad-4e1d-a4bf-fda8860ef521" />
  <img width="1600" height="1200" alt="ic33" src="https://github.com/user-attachments/assets/82061772-6f7a-4354-be7d-3f38f69ce3e7" />
  <img width="1600" height="1200" alt="final work" src="https://github.com/user-attachments/assets/fa6718b6-0915-4d87-b734-c7c44821a169" />




---
*A personal project built independently while exploring embedded systems and PCB design.* 
