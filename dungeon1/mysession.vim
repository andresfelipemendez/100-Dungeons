let SessionLoad = 1
let s:so_save = &g:so | let s:siso_save = &g:siso | setg so=0 siso=0 | setl so=-1 siso=-1
let v:this_session=expand("<sfile>:p")
silent only
silent tabonly
cd ~/development/100-Dungeons/dungeon1
if expand('%') == '' && !&modified && line('$') <= 1 && getline(1) == ''
  let s:wipebuf = bufnr('%')
endif
let s:shortmess_save = &shortmess
if &shortmess =~ 'A'
  set shortmess=aoOA
else
  set shortmess=aoO
endif
badd +17 src/generator/codegenerator.h
badd +1 src/engine/components.toml
badd +1 src/generator/test_codegenerator.c
badd +16 src/generator/codegenerator.c
badd +0 build_engine.bat
badd +60 src/generator/componentserializer.c
argglobal
%argdel
edit src/engine/components.toml
let s:save_splitbelow = &splitbelow
let s:save_splitright = &splitright
set splitbelow splitright
wincmd _ | wincmd |
vsplit
wincmd _ | wincmd |
vsplit
wincmd _ | wincmd |
vsplit
3wincmd h
wincmd w
wincmd w
wincmd w
let &splitbelow = s:save_splitbelow
let &splitright = s:save_splitright
wincmd t
let s:save_winminheight = &winminheight
let s:save_winminwidth = &winminwidth
set winminheight=0
set winheight=1
set winminwidth=0
set winwidth=1
exe 'vert 1resize ' . ((&columns * 85 + 171) / 342)
exe 'vert 2resize ' . ((&columns * 85 + 171) / 342)
exe 'vert 3resize ' . ((&columns * 84 + 171) / 342)
exe 'vert 4resize ' . ((&columns * 85 + 171) / 342)
argglobal
balt src/generator/codegenerator.h
setlocal fdm=manual
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=0
setlocal fml=1
setlocal fdn=20
setlocal fen
silent! normal! zE
let &fdl = &fdl
let s:l = 1 - ((0 * winheight(0) + 30) / 61)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 1
normal! 0
wincmd w
argglobal
if bufexists(fnamemodify("src/generator/codegenerator.c", ":p")) | buffer src/generator/codegenerator.c | else | edit src/generator/codegenerator.c | endif
if &buftype ==# 'terminal'
  silent file src/generator/codegenerator.c
endif
balt src/generator/test_codegenerator.c
setlocal fdm=manual
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=0
setlocal fml=1
setlocal fdn=20
setlocal fen
silent! normal! zE
let &fdl = &fdl
let s:l = 33 - ((32 * winheight(0) + 30) / 61)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 33
normal! 011|
wincmd w
argglobal
if bufexists(fnamemodify("src/generator/componentserializer.c", ":p")) | buffer src/generator/componentserializer.c | else | edit src/generator/componentserializer.c | endif
if &buftype ==# 'terminal'
  silent file src/generator/componentserializer.c
endif
balt src/generator/codegenerator.c
setlocal fdm=manual
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=0
setlocal fml=1
setlocal fdn=20
setlocal fen
silent! normal! zE
let &fdl = &fdl
let s:l = 62 - ((35 * winheight(0) + 30) / 61)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 62
normal! 075|
wincmd w
argglobal
if bufexists(fnamemodify("build_engine.bat", ":p")) | buffer build_engine.bat | else | edit build_engine.bat | endif
if &buftype ==# 'terminal'
  silent file build_engine.bat
endif
balt src/generator/codegenerator.h
setlocal fdm=manual
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=0
setlocal fml=1
setlocal fdn=20
setlocal fen
silent! normal! zE
let &fdl = &fdl
let s:l = 32 - ((31 * winheight(0) + 30) / 61)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 32
normal! 0
wincmd w
3wincmd w
exe 'vert 1resize ' . ((&columns * 85 + 171) / 342)
exe 'vert 2resize ' . ((&columns * 85 + 171) / 342)
exe 'vert 3resize ' . ((&columns * 84 + 171) / 342)
exe 'vert 4resize ' . ((&columns * 85 + 171) / 342)
tabnext 1
if exists('s:wipebuf') && len(win_findbuf(s:wipebuf)) == 0 && getbufvar(s:wipebuf, '&buftype') isnot# 'terminal'
  silent exe 'bwipe ' . s:wipebuf
endif
unlet! s:wipebuf
set winheight=1 winwidth=20
let &shortmess = s:shortmess_save
let &winminheight = s:save_winminheight
let &winminwidth = s:save_winminwidth
let s:sx = expand("<sfile>:p:r")."x.vim"
if filereadable(s:sx)
  exe "source " . fnameescape(s:sx)
endif
let &g:so = s:so_save | let &g:siso = s:siso_save
set hlsearch
nohlsearch
doautoall SessionLoadPost
unlet SessionLoad
" vim: set ft=vim :