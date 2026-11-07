#!/usr/bin/env python3
"""
gspasm - a TI GSPA/GSPLNK compatible assembler + linker for the TMS34010,
written to rebuild the Midway NBA JAM / TE / SE / Hangtime program ROMs from
their original 1993-1996 source code.

It understands the dialect used by Midway:
  * Midway "preasm" local labels (#label, scoped by #* section headers)
  * TI GSP assembler directives (.text/.data/.bss/.sect/.usect, .word/.long/
    .byte/.string/.field/.even/.align, .equ/.set/.asg/.eval, .if/.elseif/
    .else/.endif, .macro/.endm with :param: substitution, .loop/.endloop,
    .include/.copy, .def/.ref/.global/.globl)
  * TMS34010 instruction set with TI syntax (bit addressing, field sizes)
  * GSPLNK command files (MEMORY/SECTIONS, GROUP, fill value)

Values are bit addresses, as on the GSP.
"""
import os
import re
import sys
import struct

# ---------------------------------------------------------------------------
# errors
# ---------------------------------------------------------------------------
class AsmError(Exception):
    pass


# ---------------------------------------------------------------------------
# relocatable values: num + sum(coeff * symbolic term)
# term keys: ('sec', objid, secname) or ('ext', name)
# ---------------------------------------------------------------------------
class Val:
    __slots__ = ('num', 'terms', 'unknown')

    def __init__(self, num=0, terms=None, unknown=False):
        self.num = num
        self.terms = terms or {}
        self.unknown = unknown

    def is_abs(self):
        return not self.terms and not self.unknown

    def __repr__(self):
        return 'Val(%r,%r%s)' % (self.num, self.terms, ',?' if self.unknown else '')


def vadd(a, b, sign=1):
    t = dict(a.terms)
    for k, c in b.terms.items():
        t[k] = t.get(k, 0) + sign * c
        if t[k] == 0:
            del t[k]
    return Val(a.num + sign * b.num, t, a.unknown or b.unknown)


def u32(x):
    return x & 0xffffffff


def s32(x):
    x &= 0xffffffff
    return x - 0x100000000 if x & 0x80000000 else x


# ---------------------------------------------------------------------------
# expression parser / evaluator
# ---------------------------------------------------------------------------
TOK_RE = re.compile(r"""
    (?P<ws>\s+)|
    (?P<str>"[^"]*"|'(?:[^'\\]|\\.){1,4}')|
    (?P<num>>[0-9A-Fa-f]+|0[xX][0-9A-Fa-f]+|[0-9][0-9A-Fa-f]*[hH]\b|[01]+[bB]\b|[0-7]+[qQ]\b|[0-9]+)|
    (?P<id>[A-Za-z_.$?][A-Za-z0-9_.$?]*)|
    (?P<op><<|>>|<=|>=|==|!=|<>|[-+*/%&|^~!()<>=,\[\]])
""", re.X)

BUILTINS = {'$isreg', '$symlen', '$symcmp', '$isdefed', '$iscons', '$isname',
            '$issym', '$firstch', '$lastch', '$ismember', '$abs', '$hi', '$lo'}


def tokenize(s):
    pos = 0
    out = []
    while pos < len(s):
        m = TOK_RE.match(s, pos)
        if not m:
            raise AsmError('bad character in expression: %r' % s[pos:])
        pos = m.end()
        kind = m.lastgroup
        if kind == 'ws':
            continue
        out.append((kind, m.group(kind)))
    return out


def parse_number(t):
    if t.startswith('>'):
        return int(t[1:], 16)
    if t[:2] in ('0x', '0X'):
        return int(t[2:], 16)
    c = t[-1]
    if c in 'hH':
        return int(t[:-1], 16)
    if c in 'bB' and re.fullmatch(r'[01]+[bB]', t):
        return int(t[:-1], 2)
    if c in 'qQ':
        return int(t[:-1], 8)
    return int(t, 10)


REG_RE = re.compile(r'^(?:([aAbB])([0-9]|1[0-4])|(sp|SP|Sp|sP))$')


def is_reg(s):
    return REG_RE.match(s.strip()) is not None


class ExprParser:
    """Recursive-descent parser with C-like precedence."""
    PREC = [
        ('==', '=', '!=', '<>'),
        ('<', '<=', '>', '>='),
        ('+', '-', '|', '^'),          # TI: OR/XOR bind like addition
        ('*', '/', '%', '<<', '>>', '&'),   # TI: shifts and AND bind like multiplication
    ]

    def __init__(self, asm, text):
        self.asm = asm
        self.text = text
        self.toks = tokenize(text)
        self.i = 0

    def peek(self):
        return self.toks[self.i] if self.i < len(self.toks) else (None, None)

    def take(self):
        t = self.peek()
        self.i += 1
        return t

    def parse(self):
        v = self.binop(0)
        if self.i != len(self.toks):
            raise AsmError('junk in expression %r' % self.text)
        return v

    def binop(self, level):
        if level == len(self.PREC):
            return self.unary()
        left = self.binop(level + 1)
        while True:
            k, t = self.peek()
            if k == 'op' and t in self.PREC[level]:
                self.take()
                right = self.binop(level + 1)
                left = self.apply(t, left, right)
            else:
                return left

    def apply(self, op, a, b):
        if op == '+':
            return vadd(a, b, 1)
        if op == '-':
            return vadd(a, b, -1)
        unknown = a.unknown or b.unknown
        if not unknown and (a.terms or b.terms):
            # relocatable values in non-linear operations: defer as unknown at
            # assembly time (only legal if the result is not needed until link)
            raise AsmError('illegal relocatable operation %r in %r' % (op, self.text))
        x, y = a.num, b.num
        if unknown:
            return Val(0, {}, True)
        if op == '*':
            r = x * y
        elif op == '/':
            if y == 0:
                raise AsmError('division by zero')
            q = abs(x) // abs(y)
            r = q if (x >= 0) == (y >= 0) else -q
        elif op == '%':
            if y == 0:
                raise AsmError('division by zero')
            r = abs(x) % abs(y)
            if x < 0:
                r = -r
        elif op == '<<':
            r = x << (y & 63)
        elif op == '>>':
            r = s32(x) >> (y & 63)
        elif op == '&':
            r = x & y
        elif op == '|':
            r = x | y
        elif op == '^':
            r = x ^ y
        elif op in ('==', '='):
            r = 1 if x == y else 0
        elif op in ('!=', '<>'):
            r = 1 if x != y else 0
        elif op == '<':
            r = 1 if x < y else 0
        elif op == '<=':
            r = 1 if x <= y else 0
        elif op == '>':
            r = 1 if x > y else 0
        elif op == '>=':
            r = 1 if x >= y else 0
        else:
            raise AsmError('bad operator ' + op)
        return Val(s32(r), {}, False)

    def unary(self):
        k, t = self.peek()
        if k == 'op' and t == '-':
            self.take()
            v = self.unary()
            return Val(-v.num, {kk: -c for kk, c in v.terms.items()}, v.unknown)
        if k == 'op' and t == '+':
            self.take()
            return self.unary()
        if k == 'op' and t == '~':
            self.take()
            v = self.unary()
            if v.terms:
                raise AsmError('~ of relocatable')
            return Val(s32(~v.num), {}, v.unknown)
        if k == 'op' and t == '!':
            self.take()
            v = self.unary()
            if v.terms:
                raise AsmError('! of relocatable')
            return Val(0 if v.num else 1, {}, v.unknown)
        return self.primary()

    def primary(self):
        k, t = self.take()
        if k == 'num':
            return Val(parse_number(t))
        if k == 'str':
            body = t[1:-1]
            v = 0
            for ch in body.encode('latin-1'):
                v = (v << 8) | ch
            return Val(v)
        if k == 'op' and t == '[':
            # XY literal [Y,X]
            y = self.binop(0)
            k2, t2 = self.take()
            if t2 != ',':
                raise AsmError('bad XY literal in %r' % self.text)
            x = self.binop(0)
            k3, t3 = self.take()
            if t3 != ']':
                raise AsmError('missing ] in %r' % self.text)
            if y.terms or x.terms:
                raise AsmError('relocatable XY literal')
            return Val(s32(((y.num & 0xffff) << 16) | (x.num & 0xffff)), {}, x.unknown or y.unknown)
        if k == 'op' and t == '(':
            v = self.binop(0)
            k2, t2 = self.take()
            if t2 != ')':
                raise AsmError('missing ) in %r' % self.text)
            return v
        if k == 'id':
            if t.lower() in BUILTINS:
                return self.builtin(t.lower())
            if t == '$':
                return self.asm.dollar()
            return self.asm.symbol_value(t)
        raise AsmError('syntax error in expression %r' % self.text)

    def builtin(self, name):
        # raw argument text until matching ')'
        k, t = self.take()
        if t != '(':
            raise AsmError('expected ( after ' + name)
        depth = 1
        args = [[]]
        while True:
            k, t = self.take()
            if k is None:
                raise AsmError('unterminated builtin')
            if t == '(':
                depth += 1
            elif t == ')':
                depth -= 1
                if depth == 0:
                    break
            if t == ',' and depth == 1:
                args.append([])
                continue
            args[-1].append(t)
        sargs = [''.join(a) for a in args]
        a0 = sargs[0] if sargs else ''
        a0s = a0.strip('"')
        if name == '$isreg':
            return Val(1 if is_reg(a0s) else 0)
        if name == '$symlen':
            return Val(len(a0s))
        if name == '$symcmp':
            a1 = sargs[1].strip('"') if len(sargs) > 1 else ''
            return Val(0 if a0s == a1 else (1 if a0s > a1 else -1))
        if name == '$isdefed':
            return Val(1 if self.asm.is_defined(a0s) else 0)
        if name == '$iscons':
            try:
                parse_number(a0s)
                return Val(1)
            except Exception:
                return Val(0)
        if name in ('$isname', '$issym'):
            return Val(1 if re.fullmatch(r'[A-Za-z_.$][A-Za-z0-9_.$]*', a0s) else 0)
        if name == '$firstch':
            return Val(ord(a0s[0]) if a0s else 0)
        if name == '$lastch':
            return Val(ord(a0s[-1]) if a0s else 0)
        if name == '$abs':
            v = ExprParser(self.asm, a0).parse()
            return Val(abs(v.num), {}, v.unknown)
        raise AsmError('unsupported builtin ' + name)


# ---------------------------------------------------------------------------
# operand classification
# ---------------------------------------------------------------------------
REG_ALIASES = {}


def parse_reg(s):
    s = s.strip()
    if s in REG_ALIASES:
        s = REG_ALIASES[s]
    m = REG_RE.match(s)
    if not m:
        return None
    if m.group(3):
        return ('S', 15)
    return (m.group(1).upper(), int(m.group(2)))


def split_operands(s):
    """split on commas at depth 0, outside quotes"""
    out = []
    cur = []
    depth = 0
    q = None
    for ch in s:
        if q:
            cur.append(ch)
            if ch == q:
                q = None
            continue
        if ch in '"\'':
            q = ch
            cur.append(ch)
            continue
        if ch in '([':
            depth += 1
        elif ch in ')]':
            depth -= 1
        if ch == ',' and depth == 0:
            out.append(''.join(cur).strip())
            cur = []
            continue
        cur.append(ch)
    tail = ''.join(cur).strip()
    if tail or out:
        out.append(tail)
    return out


class Operand:
    """kinds: reg, ind (*R), postinc (*R+), predec (-*R), disp (*R(e)),
       abs (@e), imm (e), xy (*R,XY handled by caller)"""

    def __init__(self, kind, reg=None, expr=None):
        self.kind = kind
        self.reg = reg
        self.expr = expr

    def __repr__(self):
        return 'Op(%s,%s,%s)' % (self.kind, self.reg, self.expr)


def classify(s):
    s = s.strip()
    r = parse_reg(s)
    if r:
        return Operand('reg', r)
    m = re.fullmatch(r'\*\s*([A-Za-z0-9_]+)\s*\+', s)
    if m and parse_reg(m.group(1)):
        return Operand('postinc', parse_reg(m.group(1)))
    m = re.fullmatch(r'(?:-\s*\*|\*\s*-)\s*([A-Za-z0-9_]+)', s)
    if m and parse_reg(m.group(1)):
        return Operand('predec', parse_reg(m.group(1)))
    m = re.fullmatch(r'\*\s*([A-Za-z0-9_]+)\s*\((.*)\)', s)
    if m and parse_reg(m.group(1)):
        return Operand('disp', parse_reg(m.group(1)), m.group(2))
    m = re.fullmatch(r'\*\s*([A-Za-z0-9_]+)', s)
    if m and parse_reg(m.group(1)):
        return Operand('ind', parse_reg(m.group(1)))
    if s.startswith('@'):
        return Operand('abs', None, s[1:].strip())
    return Operand('imm', None, s)


# ---------------------------------------------------------------------------
# condition codes
# ---------------------------------------------------------------------------
CC = {
    'UC': 0, '': 0, 'P': 1, 'LS': 2, 'HI': 3, 'LT': 4, 'GE': 5, 'LE': 6, 'GT': 7,
    'C': 8, 'LO': 8, 'B': 8, 'NC': 9, 'HS': 9, 'NB': 9, 'EQ': 10, 'Z': 10,
    'NE': 11, 'NZ': 11, 'V': 12, 'NV': 13, 'N': 14, 'NN': 15,
    # XY compare conditions (CMPXY: N=X zero, V=X neg, Z=Y zero, C=Y neg)
    'YZ': 10, 'YNZ': 11, 'YN': 8, 'YNN': 9, 'YLT': 8, 'YGE': 9, 'YLE': 2, 'YGT': 3,
    'XZ': 14, 'XNZ': 15, 'XN': 12, 'XNN': 13, 'XLT': 12, 'XGE': 13, 'XLE': 4, 'XGT': 5,
}


# ---------------------------------------------------------------------------
# object file model
# ---------------------------------------------------------------------------
class Section:
    def __init__(self, name, kind):
        self.name = name
        self.kind = kind      # 'init' or 'uninit'
        self.size = 0         # bits
        self.data = {}        # bitpos -> (nbits, Val, signed_check)
        self.items = []       # list of (bitpos, nbits, Val, mode)
        self.base = None      # linked address
        self.align = 16       # required alignment (bits)


class ObjFile:
    def __init__(self, name, objid):
        self.name = name
        self.objid = objid
        self.sections = {}
        self.symbols = {}     # name -> Val (local)
        self.defs = set()     # exported
        self.refs = set()


# ---------------------------------------------------------------------------
# source preprocessing (Midway preasm: #local labels)
# ---------------------------------------------------------------------------
LOCAL_RE = re.compile(r'#([A-Za-z0-9_]+)')


def preasm(lines, uniq):
    """Emulate Midway PREASM: '#name' labels are local to sections that start
    at lines beginning with '#*'. Only code outside quotes/comments is touched."""
    out = []
    sec = 0
    for line in lines:
        if line.startswith('#*'):
            sec += 1
            out.append('*' + line[2:])
            continue
        if '#' not in line:
            out.append(line)
            continue
        # rewrite #label outside of strings and comments
        res = []
        q = None
        i = 0
        n = len(line)
        while i < n:
            ch = line[i]
            if q:
                res.append(ch)
                if ch == q:
                    q = None
                i += 1
                continue
            if ch == ';':
                res.append(line[i:])
                break
            if ch in '"':
                q = ch
                res.append(ch)
                i += 1
                continue
            if ch == '#':
                m = LOCAL_RE.match(line, i)
                if m:
                    res.append('L%s_%d_%s' % (uniq, sec, m.group(1)))
                    i = m.end()
                    continue
            res.append(ch)
            i += 1
        out.append(''.join(res))
    return out


# ---------------------------------------------------------------------------
# file system helpers (DOS: case-insensitive names)
# ---------------------------------------------------------------------------
_dircache = {}


def find_file(name, paths):
    name = name.strip().strip('"').strip("'").replace('\\', '/')
    for p in paths:
        cand = os.path.join(p, name)
        if os.path.isfile(cand):
            return cand
        d = os.path.dirname(cand) or '.'
        b = os.path.basename(cand).lower()
        if d not in _dircache:
            try:
                _dircache[d] = {f.lower(): f for f in os.listdir(d)}
            except OSError:
                _dircache[d] = {}
        if b in _dircache[d]:
            return os.path.join(d, _dircache[d][b])
    raise AsmError('include file not found: %s' % name)


def read_lines(path):
    with open(path, 'rb') as f:
        data = f.read()
    text = data.decode('latin-1')
    text = text.replace('\x1a', '')
    return text.replace('\r\n', '\n').replace('\r', '\n').split('\n')


# ---------------------------------------------------------------------------
# the assembler
# ---------------------------------------------------------------------------
SUBST_RE = re.compile(r':([A-Za-z_$][A-Za-z0-9_$]*)(\.S)?(?:\(([^():]*)\))?:')
IDENT_RE = re.compile(r'[A-Za-z_.$?][A-Za-z0-9_.$?]*')


class Macro:
    def __init__(self, name, params, body):
        self.name = name
        self.params = params
        self.body = body


class Assembler:
    def __init__(self, include_paths, defines=None, case_insensitive=False, opts=None):
        self.include_paths = include_paths
        self.defines = defines or {}
        self.ci = case_insensitive
        self.opts = opts or {}

    # ------------------------------------------------------------- helpers
    def key(self, name):
        return name.upper() if self.ci else name

    def here(self):
        return Val(self.sec.size, {('sec', self.obj.objid, self.sec.name): 1})

    def dollar(self):
        if self.line_start is not None and self.line_start[0] is self.sec:
            return Val(self.line_start[1], {('sec', self.obj.objid, self.sec.name): 1})
        return self.here()

    def is_defined(self, name):
        n = self.key(name)
        return n in self.symtab or n in self.obj.refs

    def symbol_value(self, name):
        k = self.key(name)
        if k in self.symtab:
            return self.symtab[k]
        if self.passno == 1:
            if self.estimating:
                if k in self.prev_labels:
                    return self.prev_labels[k]
                if self.prev_labels:
                    return Val(0, {('ext', k): 1})
            self.unresolved_in_pass1 = True
            return Val(0, {}, True)
        if k in self.obj.refs:
            return Val(0, {('ext', k): 1})
        # pass 2: undefined -> treat as external (TI requires .ref; be lenient)
        self.implicit_refs.add(k)
        return Val(0, {('ext', k): 1})

    def eval(self, text):
        text = text.strip()
        if text == '':
            return Val(0)
        return ExprParser(self, text).parse()

    def eval_abs(self, text, what='expression'):
        v = self.eval(text)
        if v.unknown:
            return None
        if v.terms:
            raise AsmError('%s must be absolute: %s' % (what, text))
        return v.num

    # ------------------------------------------------------- substitution
    def substitute(self, line, local):
        """apply :sym: forced substitution and bare-token substitution"""
        if ':' in line:
            def rep(m):
                n = m.group(1)
                if n in local:
                    val = local[n]
                elif n in self.subst:
                    val = self.subst[n]
                else:
                    return m.group(0)
                if m.group(3) is not None:
                    parts = split_operands(self.subst_bare(m.group(3), local))
                    st = self.eval_abs(parts[0]) or 1
                    ln = self.eval_abs(parts[1]) if len(parts) > 1 else 1
                    return val[st - 1:st - 1 + ln]
                return val
            for _ in range(4):
                nl = SUBST_RE.sub(rep, line)
                if nl == line:
                    break
                line = nl
        return line

    def subst_bare(self, text, local):
        if not local and not self.subst:
            return text
        out = []
        i = 0
        q = None
        n = len(text)
        while i < n:
            ch = text[i]
            if q:
                out.append(ch)
                if ch == q:
                    q = None
                i += 1
                continue
            if ch == '"':
                q = ch
                out.append(ch)
                i += 1
                continue
            if ch == "'":
                mm = re.match(r"'(?:[^'\\]|\\.){1,4}'", text[i:])
                if mm:
                    out.append(mm.group(0))
                    i += len(mm.group(0))
                    continue
            m = IDENT_RE.match(text, i)
            if m and (i == 0 or not (text[i - 1].isalnum() or text[i - 1] in '_$.')):
                w = m.group(0)
                if w.endswith('.S') and (w[:-2] in local or w[:-2] in self.subst):
                    w = w[:-2]
                if w in local:
                    out.append(local[w])
                elif w in self.subst:
                    out.append(self.subst[w])
                else:
                    out.append(w)
                i = m.end()
                continue
            out.append(ch)
            i += 1
        return ''.join(out)

    # ------------------------------------------------------- line splitting
    @staticmethod
    def strip_comment(line):
        q = None
        for i, ch in enumerate(line):
            if q:
                if ch == q:
                    q = None
                continue
            if ch == '"':
                q = ch
            elif ch == "'" and i + 2 < len(line) and line[i + 2] == "'":
                # character constant 'x'
                pass
            elif ch == ';':
                return line[:i]
        return line

    def split_line(self, line):
        """-> (label, opcode, operands)"""
        if not line.strip():
            return None, None, ''
        if line[0] == '*':
            return None, None, ''
        line = self.strip_comment(line).rstrip()
        if not line.strip():
            return None, None, ''
        label = None
        rest = line
        if line[0] not in ' \t':
            m = re.match(r'\S+', line)
            label = m.group(0)
            rest = line[m.end():]
            if label.endswith(':') and not label.startswith(':'):
                label = label[:-1]
            elif ':' in label and not label.startswith(':') and label.count(':') == 1:
                # "label:opcode" without space
                label, _, extra = label.partition(':')
                rest = extra + rest
        rest = rest.strip()
        if not rest:
            return label, None, ''
        m = re.match(r'(\S+)\s*(.*)$', rest)
        return label, m.group(1), m.group(2).strip()

    # ------------------------------------------------------- emission
    def emit(self, nbits, val, mode='u'):
        """append a field of nbits at the current section position"""
        if self.passno == 2:
            if self.sec.kind != 'init':
                raise AsmError('data in uninitialized section ' + self.sec.name)
            self.sec.items.append((self.sec.size, nbits, val, mode, self.where()))
        self.sec.size += nbits

    def emit_word(self, w):
        self.emit(16, Val(w & 0xffff))

    def where(self):
        return '%s:%d' % (self.cur_file, self.cur_line)

    def align(self, bits):
        r = self.sec.size % bits
        if r:
            pad = bits - r
            if self.sec.kind == 'init' and self.passno == 2:
                self.sec.items.append((self.sec.size, pad, Val(0), 'u', self.where()))
            self.sec.size += pad

    def set_section(self, name, kind):
        if name not in self.obj.sections:
            self.obj.sections[name] = Section(name, kind)
        self.sec = self.obj.sections[name]

    def define_label(self, name, val=None):
        k = self.key(name)
        if val is None:
            val = self.here()
        if self.passno == 1:
            if k in self.labels_p1:
                raise AsmError('duplicate label ' + name)
            self.labels_p1[k] = val
            self.symtab[k] = val
        else:
            old = self.labels_p1.get(k)
            if old is not None and (old.num != val.num or old.terms != val.terms):
                raise AsmError('label %s moved between passes (%r -> %r)' % (name, old, val))
            self.symtab[k] = val

    # ------------------------------------------------------- top level
    def assemble(self, path, objid, preasm_main=True):
        self.obj = ObjFile(os.path.basename(path), objid)
        self.macros = {}
        lines = read_lines(path)
        if preasm_main:
            lines = preasm(lines, str(objid))
        self.main_lines = lines
        self.main_path = path
        self.decisions = {}
        self.forced_long = set()
        self.decision_pc = {}
        self.line_start = None
        self.labels_p1 = {}
        self.prev_labels = {}
        self.estimating = False
        schedule = [1] * 12 + [2]
        it = 0
        while True:
            passno = schedule[min(it, len(schedule) - 1)]
            it += 1
            if passno == 1 and it > 1:
                if self.labels_p1 == self.prev_labels:
                    passno = 2
                else:
                    self.prev_labels = self.labels_p1
                    self.labels_p1 = {}
                    if it > 12:
                        raise AsmError('jump size relaxation did not converge')
            elif passno == 1:
                self.labels_p1 = {}
            self.passno = passno
            self.symtab = {}
            self.subst = dict(self.defines)
            self.macros = {}
            self.obj.sections = {}
            self.obj.defs = set()
            self.obj.refs = set()
            self.implicit_refs = set()
            self.decision_idx = 0
            self.set_section('.text', 'init')
            self.set_section('.data', 'init')
            self.set_section('.bss', 'uninit')
            self.set_section('.text', 'init')
            if passno == 2:
                # labels are known from pass 1 (for forward references)
                self.symtab = dict(self.labels_p1)
                self.p1_symtab_final = dict(self.labels_p1)
            REG_ALIASES.clear()
            self.macro_uid = 0
            self.cond = []        # stack of [active, taken, parent_active]
            self.ended = False
            self.run_lines(lines, path, {})
            # the assembler pads every initialized section to a word with zeros
            cur = self.sec
            for sec in list(self.obj.sections.values()):
                self.sec = sec
                if sec.kind == 'init':
                    self.align(16)
                else:
                    sec.size += (-sec.size) % 16
            self.sec = cur
            if passno == 2:
                break
        self.obj.symbols = self.symtab
        self.obj.refs |= self.implicit_refs
        return self.obj

    def active(self):
        return all(c[0] for c in self.cond)

    def run_lines(self, lines, fname, local, depth=0):
        if depth > 64:
            raise AsmError('macro/include nesting too deep')
        i = 0
        n = len(lines)
        saved = (getattr(self, 'cur_file', None), getattr(self, 'cur_line', 0))
        while i < n:
            if self.ended:
                break
            self.cur_file = fname
            self.cur_line = i + 1
            raw = lines[i]
            i += 1
            try:
                adv = self.process_line(raw, lines, i, fname, local, depth)
            except AsmError as e:
                raise AsmError('%s:%d: %s\n    %s' % (fname, i, e, raw.strip())) from None
            except (TypeError, ValueError, KeyError, IndexError, AttributeError) as e:
                raise AsmError('%s:%d: internal %s: %s\n    %s' % (fname, i, type(e).__name__, e, raw.strip())) from None
            if adv:
                i = adv
        self.cur_file, self.cur_line = saved

    def collect_block(self, lines, i, start_re, end_re):
        """collect lines until matching end directive (nesting aware); returns (body, next_index)"""
        depth = 1
        body = []
        while i < len(lines):
            l = lines[i]
            lab, op, _ = self.split_line(l)
            if op:
                o = op.lower()
                if re.fullmatch(start_re, o):
                    depth += 1
                elif re.fullmatch(end_re, o):
                    depth -= 1
                    if depth == 0:
                        return body, i + 1
            body.append(l)
            i += 1
        raise AsmError('unterminated block')

    def process_line(self, raw, lines, nexti, fname, local, depth):
        line = raw.replace('\t', ' ' * 1) if False else raw
        if not line.strip() or line[0] == '*':
            return None
        line = self.substitute(line, local)
        label, op, operands = self.split_line(line)
        if op is None and label is None:
            return None
        # bare substitution of the opcode field (e.g. .asg ".if 0",NOTINUSE)
        if op is not None and (op in local or op in self.subst):
            newop = local.get(op, self.subst.get(op))
            line2 = newop + ((' ' + operands) if operands else '')
            l2, op, operands = self.split_line(' ' + line2)
        lop = op.lower() if op else None

        # ---- conditional assembly (always processed)
        if lop in ('.if', '.elseif', '.else', '.endif'):
            if lop == '.if':
                parent = self.active()
                if parent:
                    c = self.cond_value(operands, local)
                    self.cond.append([c, c, True])
                else:
                    self.cond.append([False, True, False])
            elif lop == '.elseif':
                if not self.cond:
                    raise AsmError('.elseif without .if')
                top = self.cond[-1]
                if top[2] and not top[1]:
                    c = self.cond_value(operands, local)
                    top[0] = c
                    top[1] = c
                else:
                    top[0] = False
            elif lop == '.else':
                if not self.cond:
                    raise AsmError('.else without .if')
                top = self.cond[-1]
                top[0] = top[2] and not top[1]
                top[1] = True
            else:
                if not self.cond:
                    raise AsmError('.endif without .if')
                self.cond.pop()
            return None
        if not self.active():
            # skip macro definitions bodies textually too
            return None

        # ---- macro definition
        if lop == '.macro':
            if not label:
                raise AsmError('.macro without name')
            params = [p.strip() for p in split_operands(operands)] if operands else []
            body, nxt = self.collect_block(lines, nexti, r'\.macro', r'\.endm')
            self.macros[label.upper()] = Macro(label, params, body)
            return nxt
        if lop == '.loop':
            body, nxt = self.collect_block(lines, nexti, r'\.loop', r'\.endloop')
            cnt = self.eval_abs(self.subst_bare(operands, local)) if operands else 1024
            for _ in range(cnt or 0):
                self.loop_break = False
                self.run_lines(body, fname, local, depth + 1)
                if self.loop_break:
                    break
            self.loop_break = False
            return nxt
        if lop == '.break':
            c = self.cond_value(operands, local) if operands else True
            if c:
                self.loop_break = True
                self.ended_block = True
            return None

        # ---- labels
        if lop in ('.set', '.equ', 'equ', 'set', '.eval', '.asg', '.var', '.macro', '.usect'):
            pass
        elif label:
            if op is not None and (lop in ('.word', '.long', '.int', '.hword') or
                                   (not lop.startswith('.') and op.upper() not in self.macros and lop not in ('equ', 'set'))):
                self.align(16)      # the label gets the aligned address
            self.define_label(label)

        if op is None:
            return None

        # ---- macro invocation
        mac = self.macros.get(op.upper())
        if mac is not None and not op.startswith('.'):
            args = split_operands(self.subst_bare(operands, local)) if operands else []
            # TI: a quoted argument loses its quotes
            args = [a[1:-1].strip() if len(a) >= 2 and a[0] == '"' and a[-1] == '"' else a for a in args]
            nlocal = {}
            # TI: surplus arguments are folded into the last parameter (e.g. WL -1,a,b -> .long a,b)
            if mac.params and len(args) > len(mac.params):
                args = args[:len(mac.params) - 1] + [','.join(args[len(mac.params) - 1:])]
            for j, p in enumerate(mac.params):
                nlocal[p] = args[j] if j < len(args) else ''
            # label on a macro call line was already defined at current spc
            saved_cond = len(self.cond)
            self.macro_uid += 1
            uid = self.macro_uid
            body = [re.sub(r'([A-Za-z_][A-Za-z0-9_]*)\?', lambda mm: '%s$M%d' % (mm.group(1), uid), l)
                    if '?' in l else l for l in mac.body]
            self.run_lines(body, fname + '(' + mac.name + ')', nlocal, depth + 1)
            del self.cond[saved_cond:]
            return None

        self.line_start = (self.sec, self.sec.size)
        if lop.startswith('.') or lop in ('equ', 'set'):
            self.directive(lop, label, operands, local, lines, nexti, fname, depth)
            return None

        # ---- instruction
        self.line_start = (self.sec, self.sec.size)
        if self.sec.kind != 'init':
            raise AsmError('instruction in uninitialized section')
        if self.sec.size % 16:
            self.align(16)
        operands = self.trim_operand_field(self.subst_bare(operands, local))
        self.instruction(lop, operands)
        return None

    @staticmethod
    def trim_operand_field(text):
        """TI: the operand field ends at whitespace (anything after is a comment),
        except whitespace next to commas/operators."""
        t = re.sub(r'\s*,\s*', ',', text.strip())
        q = None
        depth = 0
        for i, ch in enumerate(t):
            if q:
                if ch == q:
                    q = None
                continue
            if ch == '"':
                q = ch
            elif ch in '([':
                depth += 1
            elif ch in ')]':
                depth -= 1
            elif ch in ' \t' and depth == 0:
                prev = t[:i].rstrip()[-1:]
                nxt = t[i:].lstrip()[:1]
                if prev in '+-*/|&^<>=~(' or nxt in '+-*/|&^<>=)':
                    continue
                return t[:i]
        return t

    def cond_value(self, operands, local):
        text = self.subst_bare(operands, local)
        v = self.eval(text)
        if v.unknown:
            return False
        if v.terms:
            return True
        return v.num != 0

    # ------------------------------------------------------------ directives
    def directive(self, d, label, operands, local, lines, nexti, fname, depth):
        if d in ('.equ', 'equ', '.set', 'set'):
            if not label:
                raise AsmError('%s without label' % d)
            txt = self.subst_bare(operands, local).strip()
            if is_reg(txt) or txt in REG_ALIASES:
                REG_ALIASES[label] = REG_ALIASES.get(txt, txt)
                return
            v = self.eval(txt)
            k = self.key(label)
            self.symtab[k] = v
            if self.passno == 1 and not v.unknown:
                self.labels_p1.setdefault(k, v)
            return
        if d == '.asg':
            ops = split_operands(operands)
            if len(ops) < 2:
                raise AsmError('.asg needs 2 operands')
            s = ','.join(ops[:-1])
            if len(s) >= 2 and s[0] == '"' and s[-1] == '"':
                s = s[1:-1]
            else:
                s = self.subst_bare(s, local)
            name = ops[-1].strip()
            if name in local:
                local[name] = s
            else:
                self.subst[name] = s
            return
        if d == '.eval':
            ops = split_operands(operands)
            v = self.eval(self.subst_bare(ops[0], local))
            name = ops[1].strip()
            sval = str(v.num) if not v.unknown and not v.terms else '0'
            if name in local:
                local[name] = sval
            else:
                self.subst[name] = sval
            return
        if d == '.var':
            for p in split_operands(operands):
                local[p.strip()] = ''
            return
        if d in ('.include', '.copy'):
            path = find_file(self.subst_bare(operands, local), self.include_paths)
            self.run_lines(read_lines(path), os.path.basename(path), local, depth + 1)
            return
        if d == '.mlib':
            return
        if d in ('.def', '.global', '.globl', '.ref'):
            for s in split_operands(self.subst_bare(operands, local)):
                s = s.strip()
                if not s:
                    continue
                k = self.key(s)
                if d == '.ref':
                    self.obj.refs.add(k)
                else:
                    self.obj.defs.add(k)
                    if d in ('.global', '.globl'):
                        # .global: def if defined locally, else ref (resolved at link)
                        self.obj.refs.add(k)
            return
        if d == '.text':
            self.set_section('.text', 'init')
            return
        if d == '.data':
            self.set_section('.data', 'init')
            return
        if d == '.sect':
            name = operands.strip().strip('"')
            self.set_section(name, 'init')
            return
        if d == '.bss':
            ops = split_operands(self.subst_bare(operands, local))
            name = ops[0].strip()
            size = self.eval_abs(ops[1], '.bss size') or 0
            cur = self.sec
            self.set_section('.bss', 'uninit')
            if len(ops) > 2 and ops[2].strip():
                al = self.eval_abs(ops[2]) or 0
                if al:
                    self.align(16)
            self.define_label(name)
            self.sec.size += size
            self.sec = cur
            return
        if d == '.usect':
            ops = split_operands(self.subst_bare(operands, local))
            secname = ops[0].strip().strip('"')
            size = self.eval_abs(ops[1], '.usect size') or 0
            cur = self.sec
            self.set_section(secname, 'uninit')
            if label:
                k = self.key(label)
                v = self.here()
                if self.passno == 1:
                    self.labels_p1[k] = v
                self.symtab[k] = v
            self.sec.size += size
            self.sec = cur
            return
        if d == '.even':
            self.align(16)
            return
        if d == '.align':
            # GSPA .align: align to an instruction-cache segment (512 bits). In
            # code, the gap is filled with a JRUC over NOPs.
            n = 0x200
            if operands.strip():
                n = self.eval_abs(self.subst_bare(operands, local)) or 0x200
            self.align(16)
            self.sec.align = max(self.sec.align, n)
            r = self.sec.size % n
            if r:
                pad = n - r
                if self.sec.kind == 'init':
                    words = pad // 16
                    if words >= 2:
                        self.emit_word(0xC000 | ((words - 1) & 0xff))
                        words -= 1
                    for _ in range(words):
                        self.emit_word(0x0300)
                else:
                    self.sec.size += pad
            return
        if d in ('.word', '.long', '.byte', '.hword', '.int'):
            nb = {'.word': 16, '.hword': 16, '.int': 16, '.long': 32, '.byte': 8}[d]
            ops = split_operands(self.subst_bare(operands, local))
            if nb >= 16:
                self.align(16)      # GSPA aligns .word/.long on word boundaries
            for o in ops:
                o = o.strip()
                if len(o) >= 2 and o[0] == '"' and o[-1] == '"' and d == '.byte':
                    for ch in o[1:-1].encode('latin-1'):
                        self.emit(8, Val(ch))
                    continue
                v = self.eval(o)
                self.emit(nb, v, 'w')
            return
        if d == '.string':
            ops = split_operands(self.subst_bare(operands, local))
            for o in ops:
                o = o.strip()
                if len(o) >= 2 and o[0] == '"' and o[-1] == '"':
                    for ch in o[1:-1].encode('latin-1'):
                        self.emit(8, Val(ch))
                else:
                    v = self.eval(o)
                    self.emit(8, v, 'w')
            return
        if d == '.field':
            ops = split_operands(self.subst_bare(operands, local))
            v = self.eval(ops[0])
            size = self.eval_abs(ops[1]) if len(ops) > 1 else 32
            self.emit(size, v, 'w')
            return
        if d == '.space':
            n = self.eval_abs(self.subst_bare(operands, local)) or 0
            self.emit(n, Val(0)) if n <= 64 else self._space(n)
            return
        if d == '.end':
            self.ended = True
            return
        if d in ('.file', '.title', '.width', '.option', '.mnolist', '.mlist', '.list', '.nolist',
                 '.length', '.page', '.sslist', '.ssnolist', '.drlist', '.drnolist', '.fclist',
                 '.fcnolist', '.tab', '.newblock', '.version', '.emsg', '.wmsg', '.mmsg', '.sym',
                 '.func', '.endfunc', '.label'):
            return
        raise AsmError('unknown directive ' + d)

    def _space(self, n):
        while n > 0:
            k = min(n, 32)
            self.emit(k, Val(0))
            n -= k

    # ------------------------------------------------------------ decisions
    def decide(self, compute):
        """size decisions are made in pass 1 and replayed in pass 2"""
        idx = self.decision_idx
        self.decision_idx += 1
        if self.passno == 1:
            d = compute()
            self.decisions[idx] = d
            return d
        return self.decisions[idx]

    # ------------------------------------------------------------ instructions
    def reg_bits(self, *regs):
        """returns (rfile_bit, [nums]) checking all regs in the same file (SP ok)"""
        f = None
        nums = []
        for r in regs:
            if r[0] != 'S':
                if f is None:
                    f = r[0]
                elif f != r[0]:
                    raise AsmError('registers must be in the same file')
            nums.append(r[1])
        return (0x10 if f == 'B' else 0), nums

    def field_bit(self, s):
        s = s.strip().upper()
        if s in ('0', 'W', ''):
            return 0
        if s in ('1', 'L'):
            return 1
        v = self.eval_abs(s)
        if v in (0, 1):
            return v
        raise AsmError('bad field size ' + s)

    def rel_target(self, expr):
        v = self.eval(expr)
        return v

    def instruction(self, m, operands):
        ops = split_operands(operands) if operands else []
        O = [classify(o) for o in ops]
        M = m.upper()

        def regs_only(n):
            if len(O) != n or any(o.kind != 'reg' for o in O):
                raise AsmError('%s expects %d registers' % (M, n))
            return [o.reg for o in O]

        # -- no-operand
        simple = {'EMU': 0x0100, 'POPST': 0x01C0, 'PUSHST': 0x01E0, 'NOP': 0x0300, 'CLRC': 0x0320,
                  'DINT': 0x0360, 'RETI': 0x0940, 'EINT': 0x0D60, 'SETC': 0x0DE0}
        if M in simple:
            self.emit_word(simple[M])
            return
        if M == 'RETS':
            n = self.eval_abs(ops[0]) if ops else 0
            self.emit_word(0x0960 | ((n or 0) & 0x1f))
            return
        if M == 'TRAP':
            n = self.eval_abs(ops[0]) or 0
            self.emit_word(0x0900 | (n & 0x1f))
            return
        single = {'REV': 0x0020, 'EXGPC': 0x0120, 'GETPC': 0x0140, 'JUMP': 0x0160, 'GETST': 0x0180,
                  'PUTST': 0x01A0, 'ABS': 0x0380, 'NEG': 0x03A0, 'NEGB': 0x03C0, 'NOT': 0x03E0,
                  'CALL': 0x0920}
        if M in single:
            (r,) = regs_only(1)
            rb, (n,) = self.reg_bits(r)
            self.emit_word(single[M] | rb | n)
            return
        if M in ('SEXT', 'ZEXT'):
            r = O[0].reg
            f = self.field_bit(ops[1]) if len(ops) > 1 else 0
            rb, (n,) = self.reg_bits(r)
            self.emit_word((0x0500 if M == 'SEXT' else 0x0520) | (f << 9) | rb | n)
            return
        if M == 'EXGF':
            r = O[0].reg
            f = self.field_bit(ops[1]) if len(ops) > 1 else 0
            rb, (n,) = self.reg_bits(r)
            self.emit_word(0xD500 | (f << 9) | rb | n)
            return
        if M == 'SETF':
            fs = self.eval_abs(ops[0]) or 0
            fe = self.eval_abs(ops[1]) if len(ops) > 1 else 0
            f = self.field_bit(ops[2]) if len(ops) > 2 else 0
            self.emit_word(0x0540 | (f << 9) | ((fe & 1) << 5) | (fs & 0x1f))
            return
        if M == 'PIXBLT':
            a, b = ops[0].strip().upper(), ops[1].strip().upper()
            tbl = {('L', 'L'): 0x0F00, ('L', 'XY'): 0x0F20, ('XY', 'L'): 0x0F40, ('XY', 'XY'): 0x0F60,
                   ('B', 'L'): 0x0F80, ('B', 'XY'): 0x0FA0}
            self.emit_word(tbl[(a, b)])
            return
        if M == 'FILL':
            self.emit_word(0x0FC0 if ops[0].strip().upper() == 'L' else 0x0FE0)
            return
        if M == 'LINE':
            z = self.eval_abs(ops[0]) if ops else 0
            self.emit_word(0xDF1A | ((z & 1) << 7))
            return
        if M == 'CLR':
            (r,) = regs_only(1)
            rb, (n,) = self.reg_bits(r)
            self.emit_word(0x5600 | (n << 5) | rb | n)
            return
        if M in ('INC', 'DEC'):
            (r,) = regs_only(1)
            rb, (n,) = self.reg_bits(r)
            self.emit_word((0x1000 if M == 'INC' else 0x1400) | (1 << 5) | rb | n)
            return
        # two-register ALU
        rr = {'ADD': 0x4000, 'ADDC': 0x4200, 'SUB': 0x4400, 'SUBB': 0x4600, 'CMP': 0x4800,
              'AND': 0x5000, 'ANDN': 0x5200, 'OR': 0x5400, 'XOR': 0x5600, 'DIVS': 0x5800,
              'DIVU': 0x5A00, 'MPYS': 0x5C00, 'MPYU': 0x5E00, 'LMO': 0x6A00, 'MODS': 0x6C00,
              'MODU': 0x6E00, 'ADDXY': 0xE000, 'SUBXY': 0xE200, 'CMPXY': 0xE400, 'CPW': 0xE600,
              'CVXYL': 0xE800, 'MOVX': 0xEC00, 'MOVY': 0xEE00, 'DRAV': 0xF600}
        if M in rr:
            rs, rd = regs_only(2)
            rb, (ns, nd) = self.reg_bits(rs, rd)
            self.emit_word(rr[M] | (ns << 5) | rb | nd)
            return
        if M in ('BTST', 'SLA', 'SLL', 'SRA', 'SRL', 'RL') and len(O) == 2 and O[0].kind == 'reg':
            base = {'BTST': 0x4A00, 'SLA': 0x6000, 'SLL': 0x6200, 'SRA': 0x6400, 'SRL': 0x6600, 'RL': 0x6800}[M]
            rs, rd = regs_only(2)
            rb, (ns, nd) = self.reg_bits(rs, rd)
            self.emit_word(base | (ns << 5) | rb | nd)
            return
        if M in ('BTST', 'SLA', 'SLL', 'SRA', 'SRL', 'RL'):
            k = self.eval_abs(ops[0])
            k = 0 if k is None else k
            rb, (nd,) = self.reg_bits(O[1].reg)
            if M == 'BTST':
                code = 0x1C00 | (((~k) & 0x1f) << 5)
            elif M == 'SLA':
                code = 0x2000 | ((k & 0x1f) << 5)
            elif M == 'SLL':
                code = 0x2400 | ((k & 0x1f) << 5)
            elif M == 'SRA':
                code = 0x2800 | (((-k) & 0x1f) << 5)
            elif M == 'SRL':
                code = 0x2C00 | (((-k) & 0x1f) << 5)
            else:
                code = 0x3000 | ((k & 0x1f) << 5)
            self.emit_word(code | rb | nd)
            return
        if M in ('ADDK', 'SUBK', 'MOVK'):
            k = self.eval_abs(ops[0])
            k = 1 if k is None else k
            if not (1 <= k <= 32) and not (M == 'MOVK' and k == 0):
                raise AsmError('%s constant out of range: %d' % (M, k))
            rb, (nd,) = self.reg_bits(O[1].reg)
            base = {'ADDK': 0x1000, 'SUBK': 0x1400, 'MOVK': 0x1800}[M]
            self.emit_word(base | ((k & 0x1f) << 5) | rb | nd)
            return
        if M in ('MOVI', 'ADDI', 'CMPI', 'SUBI', 'ANDI', 'ORI', 'XORI', 'ANDNI'):
            return self.immediate(M, ops, O)
        if M in ('MMTM', 'MMFM'):
            rp = O[0].reg
            rb, _ = self.reg_bits(*[o.reg for o in O if o.kind == 'reg'])
            np_ = rp[1]
            mask = 0
            for o in O[1:]:
                if o.kind != 'reg':
                    # ranges like A0-A3 are not used by Midway; keep simple
                    raise AsmError('bad register list')
                r = o.reg
                if r[0] != 'S' and (r[0] == 'B') != bool(rb):
                    raise AsmError('register list must match file')
                n = r[1]
                mask |= (1 << (15 - n)) if M == 'MMTM' else (1 << n)
            self.emit_word((0x0980 if M == 'MMTM' else 0x09A0) | rb | np_)
            self.emit_word(mask)
            return
        if M in ('CALLA',):
            self.emit_word(0x0D5F)
            self.emit(32, self.eval(ops[0]), 'w')
            return
        if M == 'CALLR':
            self.emit_word(0x0D3F)
            self.emit_rel16(ops[0])
            return
        if M == 'DSJ':
            # GSPA shortens DSJ to DSJS when the target is within 31 words
            pc = self.here()
            idx = self.decision_idx
            if '$' not in ops[1] and self.decide(lambda: self.relax(idx, ops[1], pc, 31)) == 'short':
                return self.instruction('dsjs', operands)
        if M in ('DSJ', 'DSJEQ', 'DSJNE'):
            rb, (nd,) = self.reg_bits(O[0].reg)
            base = {'DSJ': 0x0D80, 'DSJEQ': 0x0DA0, 'DSJNE': 0x0DC0}[M]
            self.emit_word(base | rb | nd)
            self.emit_rel16(ops[1])
            return
        if M == 'DSJS':
            rb, (nd,) = self.reg_bits(O[0].reg)
            tgt = self.eval(ops[1])
            pc = self.here()
            if self.passno == 2:
                d = vadd(tgt, pc, -1)
                if d.terms or d.unknown:
                    raise AsmError('DSJS target must be in same section')
                words = (d.num - 16) // 16
                if words >= 0:
                    if words > 31:
                        raise AsmError('DSJS out of range')
                    code = 0x3800 | (words << 5)
                else:
                    if -words > 31:
                        raise AsmError('DSJS out of range')
                    code = 0x3C00 | ((-words) << 5)
                self.emit_word(code | rb | nd)
            else:
                self.emit_word(0)
            return
        if M.startswith('JR') or M.startswith('JA'):
            cc = M[2:]
            if cc not in CC:
                raise AsmError('unknown condition ' + M)
            return self.jump(M[:2], CC[cc], ops[0])
        if M in ('MOVE', 'MOVB'):
            return self.move(M, ops, O)
        if M == 'PIXT':
            return self.pixt(ops)
        raise AsmError('unknown instruction ' + m)

    def relax(self, idx, expr, pc, maxw):
        """short/long decision for a relative branch, iterated to a fixed point"""
        if idx in self.forced_long:
            return 'long'
        prev_pc = self.decision_pc.get(idx)
        self.decision_pc[idx] = pc
        tgt = self.eval(expr)
        if tgt.unknown and prev_pc is not None:
            # forward reference: previous iteration's target, shifted by our growth
            self.estimating = True
            try:
                tgt = self.eval(expr)
            finally:
                self.estimating = False
            if not tgt.unknown and not tgt.terms is None:
                tgt = vadd(tgt, vadd(pc, prev_pc, -1), 1)
        d = vadd(tgt, pc, -1)
        if d.unknown:
            return 'short'
        if d.terms:
            self.forced_long.add(idx)
            return 'long'
        words = (d.num - 16) // 16
        if -maxw <= words <= maxw:
            return 'short'
        self.forced_long.add(idx)
        return 'long'

    def emit_rel16(self, expr):
        tgt = self.eval(expr)
        if self.passno == 2:
            pc = self.here()
            d = vadd(tgt, pc, -1)
            if d.terms or d.unknown:
                # relative to another section/external: link-time relocation
                self.sec.items.append((self.sec.size, 16, (tgt, pc), 'rel16', self.where()))
                self.sec.size += 16
                return
            words = (d.num - 16) // 16
            if not -32768 <= words <= 32767:
                raise AsmError('relative branch out of range')
            self.emit(16, Val(words & 0xffff))
        else:
            self.sec.size += 16

    def jump(self, kind, cc, expr):
        if kind == 'JA':
            self.emit_word(0xC080 | (cc << 8))
            self.emit(32, self.eval(expr), 'w')
            return
        # JRcc: short if target known (same section) and within range
        pc = self.here()
        idx = self.decision_idx
        size = self.decide(lambda: self.relax(idx, expr, pc, 127))
        if size == 'short':
            if self.passno == 2:
                tgt = self.eval(expr)
                d = vadd(tgt, pc, -1)
                words = (d.num - 16) // 16
                if not (-127 <= words <= 127):
                    raise AsmError('short jump out of range (%d)' % words)
                if words == 0:
                    self.emit_word(0x0300)   # GSPA: jump to next instruction -> NOP
                else:
                    self.emit_word(0xC000 | (cc << 8) | (words & 0xff))
            else:
                self.sec.size += 16
        else:
            self.emit_word(0xC000 | (cc << 8))
            self.emit_rel16(expr)

    def immediate(self, M, ops, O):
        if len(O) < 2 or O[1].kind != 'reg':
            raise AsmError('%s needs immediate,register' % M)
        expr = ops[0]
        rb, (nd,) = self.reg_bits(O[1].reg)
        force = ops[2].strip().upper() if len(ops) > 2 else None
        has_w = M in ('MOVI', 'ADDI', 'CMPI', 'SUBI')

        def compute():
            if not has_w or force == 'L':
                return 'L'
            if force == 'W':
                return 'W'
            v = self.eval(expr)
            if v.unknown or v.terms:
                return 'L'
            x = v.num
            if M in ('CMPI', 'SUBI'):
                ok = -32768 <= x <= 32767
            else:
                ok = -32768 <= x <= 32767
            return 'W' if ok else 'L'
        size = self.decide(compute)
        v = self.eval(expr)
        if M == 'MOVI':
            self.emit_word((0x09C0 if size == 'W' else 0x09E0) | rb | nd)
            self.emit(16 if size == 'W' else 32, v, 'w')
        elif M == 'ADDI':
            self.emit_word((0x0B00 if size == 'W' else 0x0B20) | rb | nd)
            self.emit(16 if size == 'W' else 32, v, 'w')
        elif M == 'CMPI':
            self.emit_word((0x0B40 if size == 'W' else 0x0B60) | rb | nd)
            self.emit(16 if size == 'W' else 32, v, 'not')
        elif M == 'SUBI':
            self.emit_word((0x0BE0 if size == 'W' else 0x0D00) | rb | nd)
            self.emit(16 if size == 'W' else 32, v, 'not')
        elif M == 'ANDI':
            self.emit_word(0x0B80 | rb | nd)
            self.emit(32, v, 'not')
        elif M == 'ANDNI':
            self.emit_word(0x0B80 | rb | nd)
            self.emit(32, v, 'w')
        elif M == 'ORI':
            self.emit_word(0x0BA0 | rb | nd)
            self.emit(32, v, 'w')
        elif M == 'XORI':
            self.emit_word(0x0BC0 | rb | nd)
            self.emit(32, v, 'w')

    def move(self, M, ops, O):
        F = 0
        args = O
        sargs = ops
        # trailing field-size operand
        if M == 'MOVE' and len(O) >= 2 and O[-1].kind == 'imm' and sargs[-1].strip().upper() in ('0', '1', 'L', 'W'):
            if len(O) == 3:
                F = self.field_bit(sargs[-1])
                args = O[:-1]
                sargs = sargs[:-1]
        if len(args) != 2:
            raise AsmError('%s needs 2 operands' % M)
        s, d = args
        b = M == 'MOVB'
        if s.kind == 'reg' and d.kind == 'reg':
            if b:
                raise AsmError('MOVB reg,reg')
            rs, rd = s.reg, d.reg
            same = rs[0] == 'S' or rd[0] == 'S' or rs[0] == rd[0]
            if same:
                rb, (ns, nd) = self.reg_bits(rs, rd)
                self.emit_word(0x4C00 | (ns << 5) | rb | nd)
            else:
                rb = 0x10 if rs[0] == 'B' else 0
                self.emit_word(0x4E00 | (rs[1] << 5) | rb | rd[1])
            return
        fb = F << 9
        if s.kind == 'reg' and d.kind == 'abs':
            rb, (ns,) = self.reg_bits(s.reg)
            self.emit_word((0x05E0 if b else (0x0580 | fb)) | rb | ns)
            self.emit(32, self.eval(d.expr), 'w')
            return
        if s.kind == 'abs' and d.kind == 'reg':
            rb, (nd,) = self.reg_bits(d.reg)
            self.emit_word((0x07E0 if b else (0x05A0 | fb)) | rb | nd)
            self.emit(32, self.eval(s.expr), 'w')
            return
        if s.kind == 'abs' and d.kind == 'abs':
            self.emit_word(0x0340 if b else (0x05C0 | fb))
            self.emit(32, self.eval(s.expr), 'w')
            self.emit(32, self.eval(d.expr), 'w')
            return
        if s.kind == 'abs' and d.kind == 'postinc':
            if b:
                raise AsmError('MOVB @,*R+ not supported')
            rb, (nd,) = self.reg_bits(d.reg)
            self.emit_word(0xD400 | fb | rb | nd)
            self.emit(32, self.eval(s.expr), 'w')
            return
        # register-indirect forms
        table = {
            ('reg', 'ind'): (0x8000, 0x8C00), ('ind', 'reg'): (0x8400, 0x8E00), ('ind', 'ind'): (0x8800, 0x9C00),
            ('reg', 'postinc'): (0x9000, None), ('postinc', 'reg'): (0x9400, None), ('postinc', 'postinc'): (0x9800, None),
            ('reg', 'predec'): (0xA000, None), ('predec', 'reg'): (0xA400, None), ('predec', 'predec'): (0xA800, None),
            ('reg', 'disp'): (0xB000, 0xAC00), ('disp', 'reg'): (0xB400, 0xAE00), ('disp', 'disp'): (0xB800, 0xBC00),
            ('disp', 'postinc'): (0xD000, None),
        }
        key = (s.kind, d.kind)
        if key not in table:
            raise AsmError('unsupported %s addressing %s,%s' % (M, s.kind, d.kind))
        wcode, bcode = table[key]
        code = bcode if b else wcode
        if code is None:
            raise AsmError('MOVB does not support %s,%s' % key)
        rb, (ns, nd) = self.reg_bits(s.reg, d.reg)
        self.emit_word(code | (0 if b else fb) | (ns << 5) | rb | nd)
        if s.kind == 'disp':
            self.emit(16, self.eval(s.expr), 'w')
        if d.kind == 'disp':
            self.emit(16, self.eval(d.expr), 'w')

    def pixt(self, ops):
        # normalize "*Rs,XY" pairs
        items = []
        i = 0
        ops = [x for o in ops for x in (o.strip()[:-3], 'XY') if True] if False else \
            [y for o in ops for y in ((o.strip()[:-3], 'XY') if o.strip().upper().endswith('.XY') else (o,))]
        while i < len(ops):
            o = ops[i].strip()
            if i + 1 < len(ops) and ops[i + 1].strip().upper() == 'XY':
                items.append((classify(o), True))
                i += 2
            else:
                items.append((classify(o), False))
                i += 1
        (s, sxy), (d, dxy) = items
        rb, (ns, nd) = self.reg_bits(s.reg, d.reg)
        if s.kind == 'reg' and d.kind == 'ind':
            code = 0xF000 if dxy else 0xF800
        elif s.kind == 'ind' and d.kind == 'reg':
            code = 0xF200 if sxy else 0xFA00
        elif s.kind == 'ind' and d.kind == 'ind':
            code = 0xF400 if (sxy or dxy) else 0xFC00
        else:
            raise AsmError('bad PIXT')
        self.emit_word(code | (ns << 5) | rb | nd)


# ---------------------------------------------------------------------------
# linker
# ---------------------------------------------------------------------------
def parse_cmd(path):
    text = open(path, encoding='latin-1').read()
    text = re.sub(r'/\*.*?\*/', ' ', text, flags=re.S)
    cmd = {'objs': [], 'memory': {}, 'sections': [], 'fill': 0xffff, 'entry': None}
    # memory block
    mm = re.search(r'MEMORY\s*\{(.*?)\}', text, re.S | re.I)
    if mm:
        for m in re.finditer(r'(\w+)\s*:\s*org\s*=\s*(0x[0-9A-Fa-f]+|\d+)\s*,\s*len\s*=\s*(0x[0-9A-Fa-f]+|\d+)', mm.group(1), re.I):
            cmd['memory'][m.group(1)] = (int(m.group(2), 0), int(m.group(3), 0))
    sm = re.search(r'SECTIONS\s*\{(.*)\}', text, re.S | re.I)
    body = sm.group(1) if sm else ''
    # parse sections: either "NAME: {} > MEM" or "GROUP: { a: {} b: {} } > MEM"
    pos = 0
    while True:
        m = re.compile(r'\s*([\w.$]+)\s*:\s*\{', re.S).match(body, pos)
        if not m:
            break
        name = m.group(1)
        # find matching brace
        depth = 1
        j = m.end()
        while depth:
            if body[j] == '{':
                depth += 1
            elif body[j] == '}':
                depth -= 1
            j += 1
        inner = body[m.end():j - 1]
        m2 = re.compile(r'\s*>\s*(\w+)').match(body, j)
        mem = m2.group(1) if m2 else None
        pos = m2.end() if m2 else j
        if name.upper() == 'GROUP':
            names = re.findall(r'([\w.$]+)\s*:\s*\{\s*\}', inner)
            cmd['sections'].append((names, mem))
        else:
            cmd['sections'].append(([name], mem))
    head = text[:mm.start()] if mm else text
    for tok in head.split('\n'):
        tok = tok.strip()
        if not tok:
            continue
        if tok.startswith('-'):
            parts = tok.split()
            if parts[0] == '-f':
                cmd['fill'] = int(parts[1], 0)
            elif parts[0] == '-e':
                cmd['entry'] = parts[1]
            continue
        for t in tok.split():
            if t.lower().endswith('.obj'):
                cmd['objs'].append(t)
    return cmd


class Linker:
    def __init__(self, objs, cmd, case_insensitive=False):
        self.objs = objs
        self.cmd = cmd
        self.ci = case_insensitive

    def link(self, align_bits=16):
        # place sections
        placed = set()
        self.globals = {}
        layout = []
        mem_next = {}
        for names, mem in self.cmd['sections']:
            org, ln = self.cmd['memory'][mem]
            addr = mem_next.get(mem, org)
            for nm in names:
                for o in self.objs:
                    s = o.sections.get(nm)
                    if s is None:
                        continue
                    al = max(align_bits, s.align)
                    if addr % al:
                        addr += al - addr % al
                    s.base = addr
                    addr += s.size
                    placed.add((o.objid, nm))
                    layout.append((nm, o.name, s.base, s.size))
            mem_next[mem] = addr
            if addr > org + ln * 8:
                raise AsmError('memory %s overflow: %x > %x' % (mem, addr, org + ln * 8))
        self.layout = layout
        self.groups = []
        for names, mem in self.cmd['sections']:
            secs = [o.sections[nm] for nm in names for o in self.objs if nm in o.sections and o.sections[nm].size]
            if secs:
                self.groups.append((min(x.base for x in secs), max(x.base + x.size for x in secs),
                                    all(x.kind == 'init' for x in secs)))
        # unplaced sections with content
        for o in self.objs:
            for nm, s in o.sections.items():
                if (o.objid, nm) not in placed:
                    if s.size:
                        raise AsmError('section %s of %s not placed by linker command file' % (nm, o.name))
                    s.base = 0
        # global symbol table
        for o in self.objs:
            for k in o.defs:
                if k in o.symbols:
                    v = o.symbols[k]
                    a = self.resolve_local(o, v)
                    if k in self.globals and self.globals[k] != a:
                        raise AsmError('duplicate global %s (%s)' % (k, o.name))
                    self.globals[k] = a

    def resolve_local(self, o, v, ext_ok=True):
        r = v.num
        for key, c in v.terms.items():
            if key[0] == 'sec':
                obj = self.objs[key[1]]
                r += c * obj.sections[key[2]].base
            else:
                name = key[1]
                if name not in self.globals:
                    raise AsmError('undefined external symbol %s (referenced in %s)' % (name, o.name))
                r += c * self.globals[name]
        return r

    def resolve_externals_late(self):
        pass

    def image(self, base, size_bytes, fill=0xff):
        img = bytearray([fill]) * size_bytes
        written = bytearray(size_bytes)
        self.relmask = bytearray(size_bytes)
        errors = []
        for o in self.objs:
            for nm, s in o.sections.items():
                if s.kind != 'init' or not s.items:
                    continue
                for (pos, nbits, val, mode, where) in s.items:
                    addr = s.base + pos
                    try:
                        if mode == 'rel16':
                            tgt, pc = val
                            t = self.resolve_local(o, tgt)
                            p = self.resolve_local(o, pc)
                            words = (t - p - 16) // 16
                            if not -32768 <= words <= 32767:
                                raise AsmError('relative branch out of range')
                            v = words & 0xffff
                        else:
                            v = self.resolve_local(o, val)
                            if mode == 'not':
                                v = ~v
                    except AsmError as e:
                        errors.append('%s: %s' % (where, e))
                        continue
                    v &= (1 << nbits) - 1 if nbits < 64 else ~0
                    self.put_bits(img, written, base, size_bytes, addr, nbits, v)
                    if mode == 'rel16' or (hasattr(val, 'terms') and val.terms):
                        b0 = (addr - base) >> 3
                        b1 = (addr - base + nbits + 7) >> 3
                        for bb in range(max(b0, 0), min(b1, size_bytes)):
                            self.relmask[bb] = 1
        # GSPLNK fills holes inside output sections with the 32-bit fill value
        fillv = self.cmd['fill'] & 0xffffffff
        for lo_, hi_, init in self.groups:
            if not init:
                continue
            a = lo_
            phase = 0           # the 32-bit fill pattern restarts at each hole
            while a < hi_:
                off = a - base
                if 0 <= off < size_bytes * 8 and not written[off >> 3]:
                    w = (fillv >> 16) if phase & 1 else (fillv & 0xffff)
                    self.put_bits(img, written, base, size_bytes, a, 16, w)
                    phase += 1
                else:
                    phase = 0
                a += 16
        return img, errors

    @staticmethod
    def put_bits(img, written, base, size_bytes, addr, nbits, v):
        off = addr - base
        if off < 0 or (off + nbits) > size_bytes * 8:
            return
        # little-endian bit order (bit address increases with significance)
        bit = off
        while nbits > 0:
            byte = bit >> 3
            sh = bit & 7
            take = min(8 - sh, nbits)
            mask = ((1 << take) - 1) << sh
            img[byte] = (img[byte] & ~mask & 0xff) | (((v & ((1 << take) - 1)) << sh) & 0xff)
            written[byte] = 1
            v >>= take
            nbits -= take
            bit += take


# ---------------------------------------------------------------------------
# driver
# ---------------------------------------------------------------------------
def build(srcdir, cmdfile, out_prefix, defines=None, include_paths=None, preasm_files=True,
          verbose=False, rom_base=0xFF800000, rom_bytes=0x100000, ci=False, ext='.ASM', asm_override=None, append_irw=None):
    cmd = parse_cmd(cmdfile)
    paths = [srcdir] + (include_paths or [])
    objs = []
    errors = 0
    for i, objname in enumerate(cmd['objs']):
        base = os.path.splitext(objname)[0]
        try:
            if base.upper() in (asm_override or ()):
                raise AsmError('override')
            src = find_file(base + ext, paths)
        except AsmError:
            src = find_file(base + '.ASM', paths)
        a = Assembler(paths, defines, case_insensitive=ci)
        if verbose:
            print('assembling', src, file=sys.stderr)
        try:
            o = a.assemble(src, i, preasm_main=preasm_files)
        except AsmError as e:
            print('ERROR', e, file=sys.stderr)
            errors += 1
            o = ObjFile(os.path.basename(src), i)
        objs.append(o)
    if errors:
        raise SystemExit('%d file(s) failed to assemble' % errors)
    ln = Linker(objs, cmd, ci)
    ln.link()
    img, errs = ln.image(rom_base, rom_bytes, cmd['fill'] & 0xff)
    for irw in (append_irw or []):
        data = open(irw, 'rb').read()
        addr, length = struct.unpack_from('<II', data, 0x2c)
        off = (addr - rom_base) // 8
        body = data[0x44:0x44 + length]
        img[off:off + len(body)] = body
    for e in errs[:50]:
        print('LINK', e, file=sys.stderr)
    if errs:
        print('%d link errors' % len(errs), file=sys.stderr)
    lo = img[0::2]
    hi = img[1::2]
    with open(out_prefix + '.lo', 'wb') as f:
        f.write(lo)
    with open(out_prefix + '.hi', 'wb') as f:
        f.write(hi)
    with open(out_prefix + '.rmask', 'wb') as f:
        f.write(ln.relmask)
    with open(out_prefix + '.lst', 'w') as f:
        for o in objs:
            for nm, sec in o.sections.items():
                if sec.base is None:
                    continue
                lastw = None
                for (pos, nbits, val, mode, where) in sec.items:
                    if where != lastw:
                        f.write('%08X %s\n' % ((sec.base + pos) & 0xffffffff, where))
                        lastw = where
    with open(out_prefix + '.map', 'w') as f:
        for nm, oname, b, sz in ln.layout:
            f.write('%-10s %-14s %08X %08X\n' % (nm, oname, b, sz))
        for k in sorted(ln.globals):
            f.write('%-32s %08X\n' % (k, ln.globals[k] & 0xffffffff))
    return ln, img, errs


if __name__ == '__main__':
    import argparse
    ap = argparse.ArgumentParser(description='GSPA-compatible assembler/linker for Midway TMS34010 sources')
    ap.add_argument('srcdir')
    ap.add_argument('cmdfile')
    ap.add_argument('out')
    ap.add_argument('-I', action='append', default=[])
    ap.add_argument('-D', action='append', default=[])
    ap.add_argument('-v', action='store_true')
    ap.add_argument('--ci', action='store_true', help='case-insensitive symbols')
    ap.add_argument('--no-preasm', action='store_true')
    ap.add_argument('--ext', default='.ASM')
    ap.add_argument('--irw', action='append', default=[], help='IRW file to place into the program ROM')
    ap.add_argument('--asm', default='', help='comma list of modules assembled from .ASM even with --ext')
    a = ap.parse_args()
    defs = {}
    for d in a.D:
        k, _, v = d.partition('=')
        defs[k] = v or '1'
    build(a.srcdir, a.cmdfile, a.out, defs, a.I, not a.no_preasm, a.v, ci=a.ci, ext=a.ext,
          asm_override=[x.strip().upper() for x in a.asm.split(',') if x.strip()], append_irw=a.irw)
