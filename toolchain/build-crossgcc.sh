#!/bin/bash

quiet=0
scriptroot="$(cd "$(dirname "$0")" && pwd)"
gnu_mirror="https://mirrors.kernel.org/gnu"
outdir=
archives=
prefixdir=
logfile=
cleanbuild=

#export CFLAGS='-static'
#export CXXFLAGS='-static'
#export LDFLAGS='-static'

function log() {
	if [[ -z $logfile ]]; then
		"$@" || exit
	elif [[ $quiet = 0 ]]; then
		"$@" | tee -a "$logfile" || exit
	else
		"$@" >> "$logfile" 2>&1 || exit
	fi
}

function err() {
	local msg="$1"
	log echo "$msg"
	exit 1
}

function fullpath() {
	readlink -f "$1"
}

function require_cmd() {
	for command in "$@"; do
		if ! which "$command" > /dev/null; then
			err "$1 is required"
		fi
	done
}

function download_file() {
	local url="$1"
	local dest="$2"

	if [[ $url = "" ]] || [[ $dest = "" ]]; then
		err 'Missing argument to download_file'
	fi

	wget -P "$dest" -N "$url" || exit
}

function require_value() {
	local option="$1"
	local message="$2"

	if [[ "$option" = "" ]]; then
		err "$message"
	fi
}

function require_values() {
	local message="$1"
	shift
	for value in "$@"; do
		require_value "$value" "$message"
	done
}

function extract_tool() {
	local base="$1"
	local ext="$2"
	local config="$3"

	local input=$(fullpath "$base$ext")

	local name=${base#$archives/}

	echo "Extracting $name"

	local src="$outdir/src"
	local build="$outdir/build"

	mkdir -p "$src" || exit
	mkdir -p "$build" || exit

	local patchname="$scriptroot/build-crossgcc-$config.patch"

	pushd "$src" || exit
	if ! [[ -d "$name" ]]; then
		tar xf "$input" || exit

		if [[ -f "$patchname" ]]; then
			log echo Applying patch $patchname
			cd "$name" || exit
			patch -p1 < "$patchname" || exit
		else
			log echo "No patch for $patchname"
		fi
	fi
	popd || exit
}

function make_tool() {
	local base="$1"
	local ext="$2"
	local target="$3"
	local config="$4"

	local name=${base#$archives/}

	local src="$outdir/src"
	local build="$outdir/build"

	build=$(fullpath "$build")
	src=$(fullpath "$src")

	mkdir -p "$build/$name" || exit
	pushd "$build/$name" || exit
	log echo Making "$target" with config "$config" and prefix "$prefixdir" in $(pwd)...
	if ! [[ -z "${@:5}" ]]
	then
		log echo "...with extra configure options: ${@:5}"
	fi

	if ! [[ -f "Makefile" ]]
	then
	set -x
		log "$src/$name/configure" --prefix="$prefixdir" $config "${@:5}" || exit
	fi
	log make $parallel $target || exit
	popd || exit
}

function process_tarball() {
	local archive="$1"
	local action="$2"
	local config="$3"
	local target="$4"

	local ext=
	local base=

	case "$archive" in
		*.tar.bz2) ext=".tar.bz2" base="${archive%%.tar.bz2}" ;;
		*.tar.gz)  ext=".tar.gz"  base="${archive%%.tar.gz}" ;;
		*)         err "Unrecognized archive extension" ;;
	esac

	log $action "$base" "$ext" "$config" "$target" "${@:5}" || exit
}

function help() {
	echo ' -a <arch list>   architectures to build (space separated list)'
	echo ' -m <url>         use specified GNU mirror'
	echo ' -j<#>            use <#> parallel workers to build'
	echo ' -o <dir>         output directory'
	echo ' -p <dir>         prefix dir'
	echo ' -c               clean the build directory'
	echo ' -q               quiet'
	echo ' -h|-?            display this help message'
	exit 1
}

# Parse arguments
while getopts a:m:j:o:cp:qh? arg
do
	case $arg in
		a ) arches="$OPTARG" ;;
		m ) gnu_mirror="$OPTARG" ;;
		j ) parallel="-j$OPTARG" ;;
		o ) outdir="$OPTARG" ;;
		p ) prefixdir="$OPTARG" ;;
		c ) cleanbuild=1 ;;
		q ) quiet=1 ;;
		h ) help ;;
		? ) help ;;
		* ) echo "unrecognized option '$arg'" ; exit 1 ;;
	esac
done

require_value "$arches" "Architecture list required, need -a <dir>"
require_value "$outdir" "Output directory required, need -o <dir>"
require_value "$prefixdir" "Prefix directory required, need -p <dir>"
require_value "$gnu_mirror" "Specified mirror URL is invalid: -m \"$gnu_mirror\""

mkdir -p "$outdir"
logfile=$(fullpath "$outdir/build.log")
prefixdir=$(fullpath "$prefixdir")

log_banner="build-crossgcc.sh log"

if ! [[ -z "$cleanbuild" ]] && [[ -f "$logfile" ]]
then
	log_head=$(head -n1 "$logfile")

	if [[ "$log_head" != "$log_banner" ]]
	then
		log echo Refusing to clean unrecognized build directory! Check -o!
		exit 1
	fi

	rm -rf "$outdir/build" || exit 1
	rm -f "$logfile" || exit 1
fi

if [[ $quiet = 0 ]]
then
	truncate --size 0 "$logfile" || exit 1
	log echo "$log_banner"
fi

require_cmd "wget" "tar" "g++" "gcc" "as" "ar" "ranlib" "nm" "tee" "tail"

# Set gccver, binver, etc...
. "$scriptroot/build-crossgcc-versions"

require_values "build-crossgcc-versions is file invalid" \
	"$gccver" "$binver" "$gdbver" "$gmpver" "$mpcver" "$mpfver"

gcctar="gcc-$gccver.tar.gz"
bintar="binutils-$binver.tar.bz2"
gdbtar="gdb-$gdbver.tar.gz"
gmptar="gmp-$gmpver.tar.bz2"
mpctar="mpc-$mpcver.tar.gz"
mpftar="mpfr-$mpfver.tar.bz2"

gccurl="$gnu_mirror/gcc/gcc-$gccver/$gcctar"
binurl="$gnu_mirror/binutils/$bintar"
gdburl="$gnu_mirror/gdb/$gdbtar"
gmpurl="$gnu_mirror/gmp/$gmptar"
mpcurl="$gnu_mirror/mpc/$mpctar"
mpfurl="$gnu_mirror/mpfr/$mpftar"

archives="$outdir/archives"
mkdir -p "$archives" || exit
download_file "$gccurl" "$archives"
download_file "$binurl" "$archives"
download_file "$gdburl" "$archives"
download_file "$gmpurl" "$archives"
download_file "$mpcurl" "$archives"
download_file "$mpfurl" "$archives"

toolre='/([^/-]+)-'
for tarball in $archives/*.tar.*; do
	log echo Extracting tarball $tarball
	if [[ $tarball =~ $toolre ]]; then
		toolname="${BASH_REMATCH[1]}"
	else
		toolname=
	fi
	process_tarball "$tarball" "extract_tool" "$toolname" || exit
done

ln -sf $(fullpath "$outdir/src/gmp-$gmpver") \
	$(fullpath "$outdir/src/gcc-$gccver/gmp") || exit
ln -sf $(fullpath "$outdir/src/mpc-$mpcver") \
	$(fullpath "$outdir/src/gcc-$gccver/mpc") || exit
ln -sf $(fullpath "$outdir/src/mpfr-$mpfver") \
	$(fullpath "$outdir/src/gcc-$gccver/mpfr") || exit

gcc_config="--target=$arches --with-system-zlib \
--enable-multilib --enable-languages=c,c++ \
--with-gnu-as --with-gnu-ld \
--enable-initfini-array \
--enable-link-mutex \
--disable-nls \
--enable-system-zlib \
--enable-multiarch \
--with-arch-32=i686 --with-abi=m64 \
--with-multilib-list=m32,m64,mx32"

# disable for now: --enable-threads=posix

bin_config="--target=$arches \
--enable-targets=x86_64-elf,i686-elf,x86_64-pe,i686-pe \
--enable-gold --enable-ld \
--enable-plugins --enable-lto
--enable-shared"

gdb_config="--target=$arches \
--with-python --with-expat --with-system-readline
--with-system-zlib --with-gnu-ld \
--enable-plugins --enable-gdbserver=no --enable-targets=all \
--enable-64-bit-bfd"

process_tarball "$archives/$bintar" "make_tool" all "$bin_config" || exit
process_tarball "$archives/$bintar" "make_tool" install "$bin_config" || exit

process_tarball "$archives/$gdbtar" "make_tool" all "$gdb_config" || exit
process_tarball "$archives/$gdbtar" "make_tool" install "$gdb_config" || exit
for target in all-gcc all-target-libgcc install-gcc install-target-libgcc; do
	process_tarball "$archives/$gcctar" "make_tool" "$target" "$gcc_config" || exit
done
