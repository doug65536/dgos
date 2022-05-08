#!/bin/bash

#set -x

arches=x86_64-dgos
quiet=0
scriptroot=$(realpath "${BASH_SOURCE%/*}")
gnu_mirror="https://mirrors.kernel.org/gnu"
outdir=gcc-build
archives=
prefixdir=
logfile=
cleanbuild=
extractonly=
enablepatch=1
parallel="-j$(which nproc >/dev/null && nproc || echo 1)"

function log() {
	if [[ -z $logfile ]]; then
		"$@" || exit
	elif [[ $quiet = 0 ]]; then
		"$@" > >(tee -a "$logfile") || exit
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
	local cmd
	for cmd in "$@"; do
		if ! which "${cmd}" > /dev/null; then
			err "${cmd} is required"
		fi
	done
}

function download_file() {
	local url="$1"
	local dest="$2"
	local filename="${url##*/}"

	if [[ $url = "" ]] || [[ $dest = "" ]]; then
		err 'Missing argument to download_file' || exit
	fi

	local dest_file="$dest/$filename"

	if ! [[ -f $dest_file ]]; then
		log "$WGET" -P "$dest" -N "$url" || return
	else
		printf "Skipping download of %s\n" "$dest"
	fi
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

	local input="$(fullpath "$base$ext")"
	local name=${base#$archives/}

	echo "Extracting $name"

	local src="$outdir/src"
	local build="$outdir/build"

	mkdir -p "$src" || exit
	mkdir -p "$build" || exit

	local patchname="$scriptroot/build-crossgcc-$config.patch"

	pushd "$src" || exit
	if ! [[ -d "$name" ]]; then
		"$TAR" xf "$input" || exit

		if [[ $enablepatch -eq 0 ]]; then
			printf "Skipping patch because extract only mode\n"
		elif [[ -f "$patchname" ]]; then
			log echo Applying patch "$patchname"
			cd "$name" || exit
			patch -p1 < "$patchname" || exit

			printf "config=%s\n" "$config"

			reconf_jobs=0
			for ((i=0; ; ++i)); do
				if [[ $i -ge $automakes_end ]]; then
					break
				fi
				local key="$config""_""$i"
				local reconf_dir="${automakes[$key]}"

				if [[ -z $reconf_dir ]]; then
					continue
				fi

				printf "key=%s\n" "$key"
				printf "Automake dir: %s\n" "$reconf_dir"

				if [[ -d $reconf_dir ]]; then
					(( ++reconf_jobs ))
					(cd "$reconf_dir" && pwd && automake) || exit
				fi
			done

#			printf "Waiting for %u jobs\n" "$reconf_jobs"
#			for ((i=0; i < reconf_jobs; ++i)); do
#				wait -n || exit
#				printf "Job completed successfully %d left\n" \
#					$(( reconf_jobs - i - 1 ))
#			done

			reconf_jobs=0
			for ((i=0 ; ; ++i)); do
				if [[ $i -ge $reconfs_end ]]; then
					break
				fi

				local key="$config""_""$i"
				local reconf_dir="${reconfs[$key]}"

				if [[ -z $reconf_dir ]]; then
					continue
				fi

				printf "key=%s\n" "$key"
				printf "Reconf dir: %s\n" "$reconf_dir"

				if [[ -d $reconf_dir ]]; then
					(( ++reconf_jobs ))

					printf "Reconfiguring in %s\n" "$reconf_dir"
					(cd "$reconf_dir" && pwd && autoreconf -f) || exit
				fi
			done

#			printf "Waiting for %u jobs\n" "$reconf_jobs"
#			for ((i=0; i < reconf_jobs; ++i)); do
#				wait -n || exit
#				printf "Job completed successfully %d left\n" \
#					$(( reconf_jobs - i - 1 ))
#			done
		else
			log echo "No patch for $patchname"
		fi
	fi
	popd || exit
}

function make_tool() {
	local base=$1
	local ext=$2
	local target=$3
	local config=$4

	local name="${base#$archives/}"

	local src="$outdir/src"
	local build="$outdir/build"

	build="$(fullpath "$build")"
	src="$(fullpath "$src")"

	mkdir -p "$build/$name" || exit
	pushd "$build/$name" || exit
	log echo Making "$target" with config "$config" \
		and prefix "$prefixdir" in cwd "$(pwd)"...
	if ! [[ -z "${@:6}" ]]; then
		log echo "...with extra configure options: ${@:6}"
	fi

	if ! [[ -f "Makefile" ]]; then
		log "$src/$name/configure" --prefix="$prefixdir" $config "${@:6}" || exit
	fi

	# Add -O to synchronize output
	log make "$parallel" "$target" || exit
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

	log "$action" "$base" "$ext" "$config" "$target" "${@:5}" || exit
}

function help() {
	echo ' -a <arch list>   architectures to build (space separated list)'
	echo ' -n               no patch'
	echo ' -m <url>         use specified GNU mirror'
	echo ' -j <#>           use <#> parallel workers to build'
	echo ' -o <dir>         output directory'
	echo ' -p <dir>         prefix dir'
	echo ' -x               extract sources only'
	echo ' -c               clean the build directory'
	echo ' -q               quiet'
	echo ' -h|-?            display this help message'
	exit 1
}

# Parse arguments
while getopts na:m:d:j:o:cxXp:qh? arg
do
	case $arg in
	    n ) enablepatch=0 ;;
		a ) arches="$OPTARG" ;;
		m ) gnu_mirror="$OPTARG" ;;
		d ) download_dir="$OPTARG" ;;
		j ) parallel="-j$OPTARG" ;;
		o ) outdir="$OPTARG" ;;
		p ) prefixdir="$OPTARG" ;;
		c ) cleanbuild=1 ;;
		q ) quiet=1 ;;
		x ) extractonly=1 ;;
		X ) extractonly=1 && enablepatch=1 ;;
		h ) help ;;
		? ) help ;;
		* ) echo "unrecognized option '$arg'" ; exit 1 ;;
	esac
done

log echo "Checking parameters"
require_value "$arches" "Architecture list required, need -a <dir>"
require_value "$outdir" "Output directory required, need -o <dir>"
if [[ -z $extractonly ]]; then
	require_value "$prefixdir" "Prefix directory required, need -p <dir>"
fi
require_value "$gnu_mirror" "Specified mirror URL is invalid: -m \"$gnu_mirror\""

mkdir -p "$outdir"
logfile=$(fullpath "$outdir/build.log")
prefixdir=$(fullpath "$prefixdir")

log_banner="build-crossgcc.bash log"

if ! [[ -z "$cleanbuild" ]] && [[ -f "$logfile" ]]
then
	log_head=$(head -n1 "$logfile")

	if [[ "$log_head" != "$log_banner" ]]
	then
		log echo "Refusing to clean unrecognized build directory! Check -o!"
		exit 1
	fi

	rm -rf "$outdir/build" || exit
	rm -f "$logfile" || exit
fi

if [[ $quiet = 0 ]]
then
	truncate --size 0 "$logfile" || exit
	log echo "$log_banner"
fi

CXX=${CXX:-g++}
CC=${CC:-gcc}
AS=${AS:-as}
AR=${AR:-gcc-ar}
RANLIB=${RANLIB:-gcc-ranlib}
NM=${NM:-gcc-nm}
WGET=${WGET:-wget}
TAR=${TAR:-tar}
TEE=${TEE:-tee}
TAIL=${TAIL:-tail}

require_cmd "$CXX" "$CC" "$AS" "$AR" "$RANLIB" \
	"$NM" "$WGET" "$TAR" "$TEE" "$TAIL"

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
[[ -n $download_dir ]] && archives=$("${REALPATH:-realpath}" "$download_dir")
mkdir -p "$archives" || exit
download_file "$gccurl" "$archives"
download_file "$binurl" "$archives"
download_file "$gdburl" "$archives"
download_file "$gmpurl" "$archives"
download_file "$mpcurl" "$archives"
download_file "$mpfurl" "$archives"

log echo Extracting tarballs...

toolre='/([^/-]+)-[^/]+$'
for tarball in "$archives"/*.tar.*; do
	log echo Extracting tarball $tarball
	if [[ $tarball =~ $toolre ]]; then
		toolname="${BASH_REMATCH[1]}"
	else
		toolname=
	fi

	if [[ -z "$toolname" ]]; then
		echo Could not deduce toolname from $tarball
		exit 1
	fi

	process_tarball "$tarball" "extract_tool" "$toolname" || exit
done

log echo Symlinking gmp, mpc, mpfr...

ln -sf $(fullpath "$outdir/src/gmp-$gmpver") \
	$(fullpath "$outdir/src/gcc-$gccver/gmp") || exit
ln -sf $(fullpath "$outdir/src/mpc-$mpcver") \
	$(fullpath "$outdir/src/gcc-$gccver/mpc") || exit
ln -sf $(fullpath "$outdir/src/mpfr-$mpfver") \
	$(fullpath "$outdir/src/gcc-$gccver/mpfr") || exit

if [[ -n $extractonly ]]; then
	log echo 'Just extracting, done'
	exit 0
fi


bin_config="--target=$arches \
--disable-nls \
--enable-gold \
--enable-ld \
--enable-plugins \
--enable-lto \
--with-sysroot \
--enable-shared \
--enable-multiarch"

if [[ $arches =~ "x86" ]]; then
	bin_config+=" --enable-targets=x86_64-dgos,x86_64-pe"
fi

gdb_config="--target=$arches \
--disable-nls \
--with-python \
--with-expat \
--with-system-readline \
--with-system-zlib \
--with-gnu-ld \
--enable-plugins \
--enable-gdbserver=no \
--enable-64-bit-bfd \
--enable-multiarch"

if [[ $arches =~ "x86" ]]; then
	gdb_config+=" --enable-targets=x86_64-dgos,i686-dgos,x86_64-pe"
fi

gcc_config="--target=$arches \
--enable-languages=c,c++ \
--enable-multilib \
--enable-multiarch \
--disable-nls \
--enable-initfini-array \
--enable-gnu-indirect-function \
--enable-__cxa_atexit \
--enable-tls \
--enable-threads \
--enable-shared \
--enable-system-zlib \
--disable-hosted-libc \
--disable-hosted-libstdcxx \
--with-system-zlib \
--without-headers \
--with-long-double-128 \
--with-readline \
--with-gnu-as \
--with-gnu-ld"

if [[ $arches =~ "x86" ]]; then
	gcc_config+=" --with-arch-32=i686 \
	--with-abi=m64 \
	--with-multilib-list=m32,m64,mx32"
fi

#--enable-threads=posix
#--disable-hosted-libstdcxx
#--disable-bootstrap


# disable for now: --enable-threads=posix

process_tarball "$archives/$bintar" "make_tool" all "$bin_config" || exit
process_tarball "$archives/$bintar" "make_tool" install "$bin_config" || exit

process_tarball "$archives/$gdbtar" "make_tool" all "$gdb_config" || exit
process_tarball "$archives/$gdbtar" "make_tool" install "$gdb_config" || exit

for target in all-gcc install-gcc \
		all-target-libgcc install-target-libgcc; do
	process_tarball "$archives/$gcctar" "make_tool" \
		"$target" "$gcc_config" || exit
done
