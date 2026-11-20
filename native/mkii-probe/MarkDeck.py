import re
import sys
from pathlib import Path

deck = Path(sys.argv[1])
text = deck.read_text()
text, count = re.subn(r'(^[^\n]*c\.bjts\.push_back\([^\n]*// x(?:left|right)\.xboard\.q(?:12|13)[ \t]*$)',
                      r'\1\n    c.bjts.back().limiterCharge=true;', text, flags=re.M)
if count != 4:
    raise ValueError(f"Expected four Q12/Q13 transistors, found {count}")
for channel, name in enumerate(("xleft", "xright")):
    text, count = re.subn(rf'(^[^\n]*c\.capacitors\.push_back\([^\n]*// {name}\.xboard\.c14[ \t]*$)',
                          rf'\1\n    c.limiterEnvelope[{channel}]=c.capacitors.back().a;',
                          text, flags=re.M)
    if count != 1:
        raise ValueError(f"Expected one {name} C14")
deck.write_text(text)
