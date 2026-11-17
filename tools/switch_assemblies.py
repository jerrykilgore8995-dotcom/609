"""Generate the mechanical switch contacts and original resistor ladders."""

from pathlib import Path


DESTINATION = Path(__file__).resolve().parents[1] / "circuit" / "switches.inc"


def ladder(name, first, values, first_tap=1, bypass_first=False):
    connections = [first] + [f"T{n}" for n in range(first_tap, first_tap + len(values))]
    lines = [f".subckt {name} W C params: POS=1 ENABLE=1"]
    if len(values) == 0:
        raise ValueError(f"Empty ladder {name}")
    for index, (a, b, value) in enumerate(zip(connections, connections[1:], values), 1):
        lines.append(f"R{index} {a} {b} {value}")
    if bypass_first:
        connections = [None, *connections[:0:-1]]
    elif first_tap == 2:
        pass
    else:
        connections = connections[1:]
    for index, tap in enumerate(connections, 1):
        if tap is not None:
            lines.append(f"RS{index} W {tap} {{(POS=={index})&&(ENABLE>0.5)?0.01:1e10}}")
    return '\n'.join([*lines, f".ends {name}"])


# The ratio network retains its ladder topology. These effective
# resistances trim the reconstructed devices against the JD 0/+10 dBu test.
ratio = ladder("PL20233", "C", [216, 399, 1285, 3725, 16875])
makeup = ladder("PL20234", "C", [130, 43, 51, 75, 91, 160, 240, 390, 820, 2400], bypass_first=True)
threshold = ladder("PL20236", "C", [240, 68, 75, 110, 130, 180, 200, 300, 360, 510, 680, 910, 1200, 2000, 3000, 5100])
limiter = ladder("PL20238", "C", [51, 62, 56, 68, 68, 68, 75, 82, 91, 91, 91, 100, 110, 110, 120, 130, 130, 150, 150, 160, 180, 180], first_tap=2)

recovery = """.subckt PL202REC W C params: POS=1 FIX1=24k FIX2=75k FIX3=150k FIX4=330k
R1 C B1 {FIX1}
R2 B1 B2 {FIX2}
R3 B2 B3 {FIX3}
R4 B3 B4 {FIX4}
C1 C AUTO 47u
R5 C AUTO 120k
R6 WA AUTO 120k
D1 DA B5 DBAX13
D2 AUTO DA DBAX13
R7 B5 B6 18k
R8 B6 AUTO 7.5k
RSA5 WA C {POS==5?0.01:1e10}
""" + '\n'.join(f"RSB{i} W B{i} {{POS=={i}?0.01:1e10}}" for i in range(1, 7)) + "\n.ends PL202REC"

DESTINATION.write_text("* Generated from handbook drawings PL20233-PL20238.\n\n" + '\n\n'.join((ratio, makeup, threshold, limiter, recovery)) + '\n')
