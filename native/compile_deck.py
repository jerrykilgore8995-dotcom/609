"""Flatten the connected 609 SPICE deck and all front-panel detents."""
import ast
from collections import Counter, defaultdict
import math
from pathlib import Path
import re
import sys

source, bias_file, dest = map(Path, sys.argv[1:4])
entry = source if source.is_file() else source / "609.cir"
top, subs, models, devices = [], {}, {}, []
scales = dict(meg=1e6, t=1e12, gig=1e9, g=1e9, k=1e3, m=1e-3, u=1e-6,
              n=1e-9, p=1e-12, f=1e-15)
literal = re.compile(r"(?<![\w.])((?:\d+\.?\d*|\.\d+)(?:e[+-]?\d+)?)((?!e[+-]?\d)[a-z]+)(?![\w.])")

def evaluate(value, scope):
    value = value.replace("&&", " and ").replace("||", " or ")
    if "?" in value:
        condition, values = value.strip().strip("{}").strip().split("?")
        positive, negative = values.split(":")
        return evaluate(positive if evaluate(condition,scope) else negative,scope)
    value = literal.sub(lambda m: repr(float(m[1]) * next(
        (v for k, v in scales.items() if m[2].startswith(k)), 1)), value.strip().strip("{}").strip())
    tree = ast.parse(value, mode="eval")
    allowed = (ast.Expression, ast.Load, ast.Constant, ast.Name, ast.BinOp,
               ast.BoolOp, ast.And, ast.Or, ast.Compare, ast.Eq, ast.Gt, ast.UnaryOp, ast.Add, ast.Sub, ast.Mult, ast.Div, ast.USub, ast.UAdd, ast.Call)
    if any(not isinstance(node, allowed) for node in ast.walk(tree)):
        raise ValueError(f"unsupported parameter expression {value!r}")
    return float(eval(compile(tree, "<circuit-parameter>", "eval"), {"__builtins__": {}},
                      dict(log10=math.log10, max=max, min=min, wiper=lambda x:min(.99999,max(.00001,x)), **scope)))

def read(path, title=False):
    lines = []
    for line in path.read_text(encoding="latin-1").splitlines()[int(title):]:
        line = line.lower().strip().strip("\x1a")
        if not line or line.startswith("*"):
            continue
        if line.startswith("+"):
            lines[-1] += " " + line[1:].strip()
        else:
            lines.append(line)
    current = top
    for line in lines:
        args = line.split()
        directive = args[0]
        if directive == ".include":
            read(path.parent / args[1])
        elif directive == ".subckt":
            split = args.index("params:") if "params:" in args else len(args)
            current = []
            subs[args[1]] = (args[2:split], args[split+1:], current)
        elif directive == ".ends":
            current = top
        elif directive not in {".end", ".options", ".func"}:
            current.append(line)

def expand(lines, prefix="", mapping=None, parent=None, override=None):
    mapping = mapping or {}
    env = dict(parent or {})
    for line in lines:
        if line.startswith(".param "):
            for field in line.split()[1:]:
                k, v = field.split("=", 1)
                env[k] = (override or {}).get(k, evaluate(v, env))
        if line.startswith(".model "):
            m = re.fullmatch(r"\.model\s+(\S+)\s+(\w+)\s*\(\s*(.*?)\s*\)", line)
            if not m:
                raise ValueError(line)
            models[prefix+m[1]] = (m[2], {k:evaluate(v,env) for k,v in
                                       re.findall(r"(\w+)\s*=\s*([^ ,]+)", m[3])})
    def node(n):
        return "0" if n == "0" else mapping.get(n, prefix+n)
    for line in lines:
        orig, *args = line.split()
        kind, name = orig[0], prefix+orig
        if orig in {".model", ".param"}:
            continue
        if kind == "x":
            split = next((i for i,v in enumerate(args) if "=" in v), len(args))
            if split and args[split-1]=="params:":
                args.pop(split-1)
                split-=1
            pins, defaults, body = subs[args[split-1]]
            controls = dict(env)
            for argument in [*defaults,*args[split:]]:
                k,v = argument.split("=", 1)
                controls[k] = evaluate(v, env)
            expand(body,name+".",dict(zip(pins,map(node,args[:split-1]),strict=True)),controls)
            continue
        pins = list(map(node,args[:3 if kind in "qj" else 2]))
        if kind == "k":
            devices.append((kind,name,[],(prefix+args[0],prefix+args[1],evaluate(args[2],env))))
            continue
        if kind == "b":
            value = None
            if name not in {"bsignal","bmains"}:
                if orig=="bavg":
                    value = "detector", [prefix+"vsense",node("sym"),node("ref"),node("ct")], [env[k] for k in ("icref","iref","vref","ksq")]
                elif orig=="bsink":
                    value = "comparator", [node(k) for k in ("out","vn","inm","inp")], []
                elif orig=="bdrive":
                    kind = "y"
                    value = ("negativeRegulator" if "xvr2" in prefix else "positiveRegulator"), [node("in"),node("ref")], []
                else:
                    raise ValueError(line)
        elif kind in "dqj":
            value = prefix+args[-1] if prefix+args[-1] in models else args[-1]
        elif kind in "rclvi":
            expr = args[3] if args[2] == "dc" else args[2]
            if kind=="r": expr=" ".join(args[2:])
            value = (0.0 if kind=="v" and expr.startswith("sin(") else evaluate(expr.lstrip(), env))
        elif kind in "egfh":
            parts = args[2:]
            if parts[0].startswith("poly("):
                count = int(parts.pop(0)[5:-1])
                if kind=="e" and not parts[0].startswith("("):
                    ctrl=[tuple(map(node,parts[i:i+2])) for i in range(0,2*count,2)]
                    parts=parts[2*count:]
                else:
                    ctrl=([tuple(map(node,v.strip("()").split(","))) for v in parts[:count]]
                          if kind=="e" else [prefix+v for v in parts[:count]])
                    parts=parts[count:]
                coeff=[evaluate(arg,env) for arg in parts]
                if len(coeff)==count+1:
                    value=ctrl,coeff[0],coeff[1:]
                elif count==1 and kind=="f" and len(coeff)==4 and coeff[:3]==[0,0,0]:
                    kind="z"
                    value=ctrl[0],coeff[3]
                else:
                    raise ValueError(line)
            elif kind in "eg":
                value = [(node(parts[0]),node(parts[1]))], 0, [evaluate(parts[2],env)]
            else:
                value = [prefix+parts[0]], 0, [evaluate(parts[1],env)]
        else:
            raise ValueError(line)
        devices.append((kind,name,pins,value))

def condense_rotaries(expanded):
    # The unloaded contacts and ladder resistors form a linear two-port.
    # Eliminate its internal nodes exactly for each position, preserving the
    # actual branch current and the loading of the open and closed contacts.
    rotaries = defaultdict(list)
    remaining = []
    for kind, name, pins, value in expanded:
        prefix = name.rsplit('.', 1)[0]
        if prefix.rsplit('.', 1)[-1] in {'xgain','xcthresh','xlthresh','xratio'}:
            rotaries[prefix].append((pins, value))
        else:
            remaining.append((kind, name, pins, value))
    for name, parts in rotaries.items():
        links = defaultdict(dict)
        def connect(a,b,g):
            if a==b:
                return
            links[a][b]=links[b][a]=links[a].get(b,0)+g
        for (a,b),r in parts:
            connect(a,b,1/r)
        for node in list(links):
            if not node.startswith(name+'.'):
                continue
            neighbors=links.pop(node)
            total=sum(neighbors.values())
            for a,g_a in neighbors.items():
                del links[a][node]
                for b,g_b in neighbors.items():
                    if a<b:
                        connect(a,b,g_a*g_b/total)
        a,b=links
        remaining.append(('r',name+'.req',[a,b],1/links[a][b]))
    return remaining


read(entry, True)
expand(top)
base_devices = condense_rotaries(devices)
face = dict(ct=range(1,17), cr=range(1,6), cm=range(1,7), cg=range(1,12),
            lt=range(1,24), lr=range(1,7), ce=range(2), le=range(2), fast=range(2))
controls = {**{key+side: positions for side in "lr" for key, positions in face.items()},
            **{key: range(2) for key in ("active", "linked", "power")},
            "catkl": range(3)}
contact_values = {}
for control_index, (control, positions) in enumerate(controls.items()):
    variants = []
    for pos in positions:
        devices.clear()
        override={"cel":1,"cer":1,"cgl":11,"cgr":11,"linked":1,"active":1,"power":1,control:pos}
        if control == "catkl":
            override["catkr"] = pos
        expand(top, override=override)
        variants.append({name:value for kind,name,_,value in condense_rotaries(devices) if kind=="r"})
    for name in variants[0]:
        values = [variant[name] for variant in variants]
        if max(values)-min(values)>max(values)*1e-12:
            contact_values.setdefault(name,[]).append((control_index, positions.start, values))
devices = base_devices
used = set()
known = {"0": (0,0)}
nodes, branches = [], []
for kind,name,pins,data in devices:
    if kind=="b" and data is None:
        continue
    if kind == "v" and pins[1] == "0" and name not in used:
        known[pins[0]] = ({"sl":1,"sr":2}.get(pins[0],0),data)
    elif kind in "vehly":
        branches.append(name)
extra = {}
for kind,name,pins,data in devices:
    ctrls = [n for pair in data[0] for n in pair] if kind in "eg" else []
    for node in [*pins,*ctrls]:
        if node not in known and node not in nodes:
            nodes.append(node)
    if kind in "dqj":
        keys = {"q": ("rc","rb","re"), "j": ("rd","none","rs"), "d": ("rs",)}[kind]
        labels = {"q": ("collector","base","emitter"), "j": ("drain","gate","source"), "d": ("pos",)}[kind]
        for key,node,label in zip(keys,pins,labels):
            if models[data][1].get(key):
                extra[name+"#"+label] = node
                nodes.append(name+"#"+label)

variables = nodes+branches
index = {n:i for i,n in enumerate(variables)}
known_index = {n:-i-1 for i,n in enumerate(known)}
def ix(node):
    return index[node] if node in index else known_index[node]
def spice_name(name):
    if "." not in name:
        return name
    path,local = name.rsplit(".",1)
    return f"{local[0]}.{path}.{local}"

header, values = bias_file.read_text(encoding="latin-1").split("Values:",1)
vectors = re.findall(r"(?m)^\s*\d+\s+([vi]\([^\n]+?\))\s+\w+\s*$",header)
values = [float(s) for s in values.split()[1:]]
count = int(re.search(r"No. Variables: (\d+)",header)[1])
assert len(values) >= count and len(vectors) in {count,count-1}
op = dict(zip(vectors,values[-len(vectors):]))
def bias(name):
    if name in extra:
        instance,pin = name.split("#")
        ext = extra[name]
        return op.get(f"v({spice_name(instance)}#{pin})",
                      op.get(f"v({ext})",known.get(ext,(0,0))[1]))
    if name in nodes:
        return op[f"v({name})"]
    branch = spice_name(name)
    return op.get(f"i({branch})", op.get(f"i({branch}#branch)", op.get("i(a$poly$"+branch+")",0.)))

def fmt(x):
    return format(x,".17g")

def channel_owner(name):
    left_ports = {"ilh", "ill", "olh", "oll", "lctl", "meterl"}
    right_ports = {"irh", "irl", "orh", "orl", "rctl", "meterr"}
    if name.startswith("xleft.") or name in left_ports:
        return 0
    if name.startswith("xright.") or name in right_ports:
        return 1
    raise ValueError(f"Unknown channel for {name}")


cpp = ["// Generated from 609.cir and the schematic transcriptions.",
       "inline void loadDeck(Circuit& c) {", f"    c.nodes = {len(nodes)};",
       f"    c.size = {len(variables)};",
       f"    c.output = {{{ix('olh')},{ix('oll')},{ix('orh')},{ix('orl')}}};",
       f"    c.meter = {{{ix('meterl')},{ix('meterr')}}};",
       "    c.channel = {" + ", ".join(str(channel_owner(k)) for k in variables) + "};",
       "    c.bias = {" + ", ".join(fmt(bias(k)) for k in variables) + "};"]
for kind,value in known.values():
    cpp.append(f"    c.constants.push_back({{{kind}, {fmt(value)}}});")
sets = {"d":("is n rs cjo vj m tt fc bv ibv",[1e-14,1,0,0,1,.5,0,.5,0,0]),
        "q":("is bf nf vaf ikf ise ne br nr var ikr isc nc rb rc re cje vje mje cjc vjc mjc tf tr fc",
             [1e-16,100,1,0,0,0,1.5,1,1,0,0,0,2,0,0,0,0,.75,.33,0,.75,.33,0,0,.5]),
        "j":("vto beta is lambda rd rs cgs cgd pb fc",[-2,1e-4,1e-14,0,0,0,0,0,1,.5])}
types = {"d":{"d"},"q":{"npn","pnp"},"j":{"njf","pjf"}}
groups = defaultdict(list)
inductors={name:data for kind,name,pins,data in devices if kind=="l"}
filtered=[device for device in devices if device[0]!="k" and not (device[0]=="b" and device[3] is None)]
for kind,name,pins,data in filtered:
    a,b = map(ix,pins[:2])
    def add(list_name,values):
        cpp.append("    c."+list_name+".push_back({"+values+"}); // "+name)
    if kind=="v" and name not in used and pins[1]=="0":
        continue
    if kind=="r":
        keys = contact_values.get(name, [])
        if not data:
            raise ValueError((name,pins,data))
        if keys:
            # A relay opens when either enabling condition is disabled. The
            # compression makeup ladder uses the same contact mask.
            tables = [(control,first,[1/v for v in values]) for control,first,values in keys]
            conditions = ", ".join("{" + f"{ctrl},{first}," + "{" + ",".join(map(fmt,values)) + "}}"
                                   for ctrl,first,values in tables)
            invert = "true" if name.rsplit(".",1)[-1].startswith("rrelay_b") else "false"
            add("contact",f"{a}, {b}, {fmt(1/data)}, {invert}, {{{conditions}}}")
        else:
            add("resistors",f"{a}, {b}, {fmt(1/data)}, 0")
    elif kind in "ci":
        add("capacitors" if kind=="c" else "currents",f"{a}, {b}, {fmt(data)}")
    elif kind=="v":
        add("sources",f"{a}, {b}, {ix(name)}, {fmt(data)}")
    elif kind=="l":
        k=ix(name)
        add("sources",f"{a}, {b}, {k}, 0")
        add("inductance",f"{k}, {k}, {fmt(data)}")
    elif kind in "byz":
        if kind=="z":
            ctrl,gain=data
            add("cubic",f"{a}, {b}, {ix(ctrl)}, {fmt(gain)}")
        else:
            model,controls,params=data
            if kind=="y":
                k=ix(name)
                add("sources",f"{a}, {b}, {k}, 0")
                a,b=k,ix("0")
            indices=", ".join(map(str,[ix(key) for key in controls]+[-1]*(4-len(controls))))
            values=", ".join(fmt(x) for x in params)
            add("behavioral",f"Behavior::{model}, {a}, {b}, {{{indices}}}, {{{values}}}")
    elif kind in "egfh":
        ctrls,offset,gains=data
        multiplier=1
        if kind in "eh":
            k=ix(name)
            add("sources",f"{a}, {b}, {k}, {fmt(offset)}")
            a,b,multiplier=k,ix("0"),-1
        elif offset:
            add("currents",f"{a}, {b}, {fmt(offset)}")
        for ctrl,gain in zip(ctrls,gains,strict=True):
            if gain:
                p,m=(ix(ctrl),ix("0")) if kind in "fh" else map(ix,ctrl)
                add("controlled",f"{a}, {b}, {p}, {m}, {fmt(gain*multiplier)}")
    else:
        model,md=models[data]
        keys,defaults=sets[kind]
        keys=keys.split()
        assert model in types[kind] and set(md)<=set(keys),(name,model,set(md)-set(keys))
        indices=list(map(ix,pins))
        series={"q":(("rc","collector"),("rb","base"),("re","emitter")), "d":(("rs","pos"),), "j":(("rd","drain"),("none","gate"),("rs","source"))}[kind]
        for i,(key,label) in enumerate(series):
            if md.get(key):
                inner=ix(name+"#"+label)
                add("resistors",f"{indices[i]}, {inner}, {fmt(1/md[key])}, 0")
                indices[i]=inner
        sign = -1 if model in {"pnp","pjf"} else 1
        model_values=", ".join(fmt(md.get(k,d)) for k,d in zip(keys,defaults,strict=True)
                               if k not in {"rs","rd","rc","rb","re"})
        groups[(kind,model_values,sign)].append((indices,name))
        if kind=="j":
            for cap,terminal in (("cgs",indices[2]),("cgd",indices[0])):
                if md.get(cap):
                    charge=", ".join(map(fmt,(0,1,md[cap],md.get("pb",1),.5,0,md.get("fc",.5),0,0)))
                    groups[("d",charge,1)].append(([indices[1],terminal],name+"#"+cap))
for kind,name,_,data in devices:
    if kind=="k":
        a,b,k=data
        mutual=fmt(k*math.sqrt(inductors[a]*inductors[b]))
        cpp.append(f"    c.inductance.push_back({{{ix(a)}, {ix(b)}, {mutual}}});")
        cpp.append(f"    c.inductance.push_back({{{ix(b)}, {ix(a)}, {mutual}}});")
offsets=defaultdict(int)
calls=defaultdict(list)
variants={"d":("diodes","DModel"),"q":("bjts","QModel"),"j":("jfets","JModel")}
for (kind,model,sign),members in groups.items():
    container,model_type=variants[kind]
    for terminals,name in members:
        cpp.append(f"    c.{container}.push_back({{{', '.join(map(str,terminals))}}}); // {name}")
    params=f"{model_type}{{{model}}}" if kind=="d" else f"{model_type}{{{model}}}, {sign}"
    calls[kind].append(f"    stamp.template operator()<{params}>({offsets[kind]}, {len(members)});")
    offsets[kind]+=len(members)
cpp.append("}")
for kind,(container,_) in variants.items():
    cpp+=["template<class Stamp>", f"inline void stamp_{container}(Stamp&& stamp) {{", *calls[kind], "}"]
dest.parent.mkdir(parents=True,exist_ok=True)
dest.write_text("\n".join(cpp)+"\n")
print("Components:",dict(Counter(row[0] for row in devices)),
      "unknowns:",len(variables),"fixed voltages:",len(known))
