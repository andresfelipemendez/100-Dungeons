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
badd +35 src/generator/codegenerator.h
badd +2 src/engine/components.toml
badd +113 src/generator/test_codegenerator.c
badd +168 src/generator/codegenerator.c
badd +41 build_engine.bat
badd +98 src/generator/componentserializer.c
badd +42 build.c
badd +30 build.h
badd +24 ~/bin/connectsoundbar.bat
badd +640 ~/AppData/Local/nvim/init.lua
badd +1 build.log
badd +1 \+\ CategoryInfo\ \ \ \ \ \ \ \ \ \ :\ NotSpecified:\ (In\ file\ include...nets.gen.cpp
badd +122 src/engine/ecs.h
badd +69 src/engine/components.gen.h
badd +52 src/engine//engine.cpp
badd +0 src/engine/memory.h
argglobal
%argdel
edit src/engine/memory.h
let s:save_splitbelow = &splitbelow
let s:save_splitright = &splitright
set splitbelow splitright
wincmd _ | wincmd |
vsplit
wincmd _ | wincmd |
vsplit
2wincmd h
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
exe 'vert 1resize ' . ((&columns * 159 + 240) / 480)
exe 'vert 2resize ' . ((&columns * 160 + 240) / 480)
exe 'vert 3resize ' . ((&columns * 159 + 240) / 480)
argglobal
balt src/engine//engine.cpp
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
let s:l = 6 - ((5 * winheight(0) + 38) / 76)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 6
normal! 034|
wincmd w
argglobal
if bufexists(fnamemodify("build.c", ":p")) | buffer build.c | else | edit build.c | endif
if &buftype ==# 'terminal'
  silent file build.c
endif
balt src/engine/components.toml
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
let s:l = 42 - ((41 * winheight(0) + 38) / 76)
if s:l < 1 | let s:l = 1 | endif
keepjumps exe s:l
normal! zt
keepjumps 42
normal! 05|
wincmd w
argglobal
enew
balt ~/AppData/Local/nvim/init.lua
setlocal fdm=manual
setlocal fde=0
setlocal fmr={{{,}}}
setlocal fdi=#
setlocal fdl=0
setlocal fml=1
setlocal fdn=20
setlocal fen
wincmd w
exe 'vert 1resize ' . ((&columns * 159 + 240) / 480)
exe 'vert 2resize ' . ((&columns * 160 + 240) / 480)
exe 'vert 3resize ' . ((&columns * 159 + 240) / 480)
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
