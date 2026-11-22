#!/usr/bin/env python3
"""Prepare an instrumented copy of the authenticated GNU grep 3.12 source.

The upstream tree remains untouched.  The runtime owns checkpoint semantics;
these calls run in ordinary control flow, never in a signal handler.
"""
import hashlib
import json
from pathlib import Path
import shutil
import sys

INPUTS = {
    'lib/getprogname.c': '0965f6d36a8a1996c6f06f9735e8cc595851b703422a9efa4e7d71d278a4777b',
    'src/grep.c': '22ff6b93485baa033ebd3469ded5695d49b5c9aba89dcdb41cc67237a7ad94a0',
    'src/kwset.c': 'b610063c6ad6e445d5aca87e479d4fbeab907d84889dc361d1657479825e110a',
    'lib/dfa.c': 'c3a1a5f0f3399018fe63265b6f5caf8fdc5ba0ff00931bd11f96005d9e218864',
    'lib/regcomp.c': '9e897576ec5421fbd8b476fb287f323eb61612c85d2ec78b88b4bc508371380e',
    'lib/regexec.c': '5cf9d377abdcc8be6469690c9f9454f468257bd07200e37c23acaa4ee587944b',
}

# The exact loop headers are also checked independently of the source hashes.
# Inserting into the controlling expression preserves loop and continue semantics.
LOOPS = {
    'src/grep.c': [
        ('for (char *p = beg; p < lim; p = endp)', 'for (char *p = beg; (srvr_grep_checkpoint (), p < lim); p = endp)'),
        ('while ((ent = fts_read (fts)))', 'while ((srvr_grep_checkpoint (), (ent = fts_read (fts))))'),
        ('while ((nbytes = safe_read (fd, buffer, bufalloc)))', 'while ((srvr_grep_checkpoint (), (nbytes = safe_read (fd, buffer, bufalloc))))'),
    ],
    'src/kwset.c': [
        ('static inline ptrdiff_t _GL_ATTRIBUTE_PURE\nbmexec_trans', 'static inline ptrdiff_t\nbmexec_trans'),
        ('for (; s < slim; s++)', 'for (; (srvr_grep_checkpoint (), s < slim); s++)'),
        ('while (++i <= d)', 'while ((srvr_grep_checkpoint (), ++i <= d))'),
        ('while (len--)', 'while ((srvr_grep_checkpoint (), len--))'),
        ('for (curr = last = kwset->trie; curr; curr = curr->next)\n    {', 'for (curr = last = kwset->trie; (srvr_grep_checkpoint (), curr); curr = curr->next)\n    {'),
        ('for (ep = text + size - 11 * len; tp <= ep; )', 'for (ep = text + size - 11 * len; (srvr_grep_checkpoint (), tp <= ep); )'),
        ('while (d <= ep - tp)', 'while ((srvr_grep_checkpoint (), d <= ep - tp))'),
        ('while (! (trie = kwset->next[c = tr (trans, *tp++)]))', 'while ((srvr_grep_checkpoint (), ! (trie = kwset->next[c = tr (trans, *tp++)])))'),
        ('while (tp < lim)', 'while ((srvr_grep_checkpoint (), tp < lim))'),
    ],
    'lib/dfa.c': [
        ('while (dfa->parse.tok != RPAREN && dfa->parse.tok != OR\n         && dfa->parse.tok >= 0)', 'while ((srvr_grep_checkpoint (), dfa->parse.tok != RPAREN && dfa->parse.tok != OR\n         && dfa->parse.tok >= 0))'),
        ('while (dfa->parse.tok == OR)', 'while ((srvr_grep_checkpoint (), dfa->parse.tok == OR))'),
        ('for (idx_t i = 0; i < tindex; i++)\n    {\n      switch (d->tokens[i])', 'for (idx_t i = 0; (srvr_grep_checkpoint (), i < tindex); i++)\n    {\n      switch (d->tokens[i])'),
        ('while ((t = trans[s]) != NULL)', 'while ((srvr_grep_checkpoint (), (t = trans[s]) != NULL))'),
        ('while (t[*p] == s)', 'while ((srvr_grep_checkpoint (), t[*p] == s))'),
        ('while (t[*p] == s1)', 'while ((srvr_grep_checkpoint (), t[*p] == s1))'),
    ],
    'lib/regcomp.c': [
        ('while (token->type != OP_ALT && token->type != END_OF_RE\n\t && (nest == 0 || token->type != OP_CLOSE_SUBEXP))', 'while ((srvr_grep_checkpoint (), token->type != OP_ALT && token->type != END_OF_RE\n\t && (nest == 0 || token->type != OP_CLOSE_SUBEXP)))'),
        ('while (token->type == OP_ALT)', 'while ((srvr_grep_checkpoint (), token->type == OP_ALT))'),
        ('for (node = 0; node < dfa->nodes_len; ++node)\n    switch (dfa->nodes[node].type)', 'for (node = 0; (srvr_grep_checkpoint (), node < dfa->nodes_len); ++node)\n    switch (dfa->nodes[node].type)'),
        ('for (src = 0; src < dfa->nodes_len; ++src)', 'for (src = 0; (srvr_grep_checkpoint (), src < dfa->nodes_len); ++src)'),
        ('for (node = root; ; )\n    {\n      /* Descend down the tree', 'for (node = root; (srvr_grep_checkpoint (), 1); )\n    {\n      /* Descend down the tree'),
    ],
    'lib/regexec.c': [
        ('while (__glibc_likely (match_first < right_lim)\n\t\t && !fastmap[t[(unsigned char) string[match_first]]])', 'while ((srvr_grep_checkpoint (), __glibc_likely (match_first < right_lim)\n\t\t && !fastmap[t[(unsigned char) string[match_first]]]))'),
        ('while (__glibc_likely (match_first < right_lim)\n\t\t && !fastmap[(unsigned char) string[match_first]])', 'while ((srvr_grep_checkpoint (), __glibc_likely (match_first < right_lim)\n\t\t && !fastmap[(unsigned char) string[match_first]]))'),
        ('while (match_first >= left_lim)', 'while ((srvr_grep_checkpoint (), match_first >= left_lim))'),
        ('for (;; match_first += incr)', 'for (; (srvr_grep_checkpoint (), 1); match_first += incr)'),
        ('while (!re_string_eoi (&mctx->input))', 'while ((srvr_grep_checkpoint (), !re_string_eoi (&mctx->input)))'),
        ('for (idx = pmatch[0].rm_so; idx <= pmatch[0].rm_eo ;)', 'for (idx = pmatch[0].rm_so; (srvr_grep_checkpoint (), idx <= pmatch[0].rm_eo) ;)'),
        ('for (sub_top_idx = 0; sub_top_idx < mctx->nsub_tops; ++sub_top_idx)', 'for (sub_top_idx = 0; (srvr_grep_checkpoint (), sub_top_idx < mctx->nsub_tops); ++sub_top_idx)'),
        ('for (sub_last_idx = 0; sub_last_idx < sub_top->nlasts; ++sub_last_idx)', 'for (sub_last_idx = 0; (srvr_grep_checkpoint (), sub_last_idx < sub_top->nlasts); ++sub_last_idx)'),
        ('for (; sl_str <= bkref_str_idx; ++sl_str)', 'for (; (srvr_grep_checkpoint (), sl_str <= bkref_str_idx); ++sl_str)'),
    ],
}


# Only mark progress after the loop condition succeeds, in actual engine work.
PROGRESS = {
    'src/kwset.c': [
        ('while ((srvr_grep_checkpoint (), len--))\n    {', 'while ((srvr_grep_checkpoint (), len--))\n    {\n      srvr_grep_progress (67);'),
        ('for (ep = text + size - 11 * len; (srvr_grep_checkpoint (), tp <= ep); )\n      {', 'for (ep = text + size - 11 * len; (srvr_grep_checkpoint (), tp <= ep); )\n      {\n        srvr_grep_progress (77);'),
    ],
    'lib/dfa.c': [
        ('for (idx_t i = 0; (srvr_grep_checkpoint (), i < tindex); i++)\n    {\n      switch (d->tokens[i])', 'for (idx_t i = 0; (srvr_grep_checkpoint (), i < tindex); i++)\n    {\n      srvr_grep_progress (67);\n      switch (d->tokens[i])'),
        ('while ((srvr_grep_checkpoint (), t[*p] == s))\n                    p++;', 'while ((srvr_grep_checkpoint (), t[*p] == s))\n                    {\n                      srvr_grep_progress (77);\n                      p++;\n                    }'),
    ],
    'lib/regcomp.c': [
        ('for (src = 0; (srvr_grep_checkpoint (), src < dfa->nodes_len); ++src)\n    {', 'for (src = 0; (srvr_grep_checkpoint (), src < dfa->nodes_len); ++src)\n    {\n      srvr_grep_progress (67);'),
    ],
    'lib/regexec.c': [
        ('while ((srvr_grep_checkpoint (), !re_string_eoi (&mctx->input)))\n    {', 'while ((srvr_grep_checkpoint (), !re_string_eoi (&mctx->input)))\n    {\n      srvr_grep_progress (77);'),
    ],
}


def replace_once(text, old, new, label):
    count = text.count(old)
    if count != 1:
        raise SystemExit(f'{label}: expected one anchor, found {count}: {old!r}')
    return text.replace(old, new, 1)


def main():
    if len(sys.argv) != 3:
        raise SystemExit('usage: prepare.py UPSTREAM OUTPUT')
    source, output = (Path(arg).resolve() for arg in sys.argv[1:])
    if source == output or source in output.parents or output in source.parents:
        raise SystemExit('upstream and output must be separate directories')
    if output.exists() and any(output.iterdir()):
        raise SystemExit('output must be absent or empty')
    transformed = {}
    for name, digest in INPUTS.items():
        data = (source / name).read_bytes()
        if hashlib.sha256(data).hexdigest() != digest:
            raise SystemExit(f'{name}: unexpected GNU grep source digest')
        text = data.decode('utf-8')
        for old, new in LOOPS.get(name, []):
            text = replace_once(text, old, new, name)
        for old, new in PROGRESS.get(name, []):
            text = replace_once(text, old, new, name + ' progress')
        if name == 'lib/getprogname.c':
            text = replace_once(text, '#  error "getprogname module not ported to this OS"',
                                '  extern const char *srvr_grep_program_name;\n  return last_component (srvr_grep_program_name);', name)
        if name == 'src/grep.c':
            text = replace_once(text, '  if (! stdout_errno)\n    close_stdout ();',
                                '  if (! stdout_errno && fclose (stdout) != 0)\n    {\n      error (0, errno, _("write error"));\n      _exit (EXIT_TROUBLE);\n    }', name)
            text = replace_once(text, '  if (err == ELOOP || err == EMLINK)\n    return true;',
                                '  (void) err;\n#ifdef ELOOP\n  if (err == ELOOP)\n    return true;\n#endif\n#ifdef EMLINK\n  if (err == EMLINK)\n    return true;\n#endif', name)
            text = replace_once(text, '  if (stdout_errno)\n    die (EXIT_TROUBLE, stdout_errno, _("write error"));',
                                '  srvr_grep_checkpoint ();\n  if (stdout_errno)\n    die (EXIT_TROUBLE, stdout_errno, _("write error"));', name)
            text = replace_once(text, '\nmain (int argc, char **argv)', '\nsrvr_grep_main (int argc, char **argv)', name)
        transformed[name] = 'extern void srvr_grep_checkpoint (void);\nextern void srvr_grep_progress (int);\n' + text
    shutil.copytree(source, output, dirs_exist_ok=True, symlinks=True,
                    ignore=shutil.ignore_patterns('.git'))
    for name, text in transformed.items():
        (output / name).write_text(text, encoding='utf-8')
    manifest = {'version': '3.12', 'inputs_sha256': INPUTS,
                'outputs_sha256': {name: hashlib.sha256((output / name).read_bytes()).hexdigest() for name in INPUTS}}
    (output / '.srvros-prepared.json').write_text(json.dumps(manifest, indent=2, sort_keys=True) + '\n')


if __name__ == '__main__':
    main()
