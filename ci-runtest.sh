#!/bin/sh
#
# This script only executes test mode
#
cd `dirname $0`
export PHANTOM_HOME=`pwd`

GDB_PORT=1235		# get rid of stalled instance by incrementing port no.
GDB_OPTS="-gdb tcp::$GDB_PORT"
#GDB_OPTS="-s"
TEST_DIR=run/test	# was oldtree/run_test
TFTP_PATH=../fat/boot
DISK_IMG=phantom.img
WARN=1
LOGFILE=make.log	# start with build log
WAIT_LAUNCH=60		# consider Phantom stalled after 1 minute if no activity in the serial0.log
PANIC_AFTER=600		# abort test after 10 minutes (consider stalled)

if [ -x /usr/libexec/qemu-kvm ] 	# CentOS check
then
	QEMU=/usr/libexec/qemu-kvm
	QEMU_SHARE=/usr/share/qemu-kvm
else
	QEMU=`which qemu || which kvm`
	QEMU_SHARE=/usr/share/qemu
fi

if [ $# -gt 0 ]
then
	while [ $# -gt 0 ]
	do
		case "$1" in
		-c)	DO_CLEANING=1		;;
		-f)	FOREGROUND=1		;;
		-s)	unset WARN		;;
		-u)	UNATTENDED=-unattended	;;
		-ng)	unset DISPLAY		;;
		*)
			echo "Usage: $0 [-u|-f] [-c] [-s] [-ng]
	-c	- run 'make clean' first
	-f	- run in foreground
	-u	- run unattended (don't stop on panic for gdb)
	-s	- suppress build warnings
	-ng	- do not show qemu/kvm window
"
			exit 0
		;;
		esac
		shift
	done
else
	CRONMODE=1
	unset DISPLAY
fi


die ( ) {
	[ -s $LOGFILE ] && {
		# submit all details in CI, show pre-failure condition interactively
		[ "$CRONMODE" ] && sed 's/^[[^m]*m//g;s/^M//g' $LOGFILE || tail $LOGFILE
	}
	[ "$1" ] && echo "$*"
	exit 1
}

[ "$DO_CLEANING" ] && make clean

make all > $LOGFILE 2>&1 || die "Build failure"
grep -B1 'error:\|] Error' $LOGFILE && {
	grep '^--- kernel build finished' $LOGFILE || die "Build failure"
}

[ "$WARN" ] && {
	echo Warnings:
	grep : $LOGFILE
}

# now test building of Phantom library
if (make -C plib > $LOGFILE 2>&1)
then
	[ "$WARN" ] && {
		echo Successfully built Phantom library
		grep Warning: $LOGFILE
	}
else
	echo "Phantom library build failure:"
	tail -20 $LOGFILE
fi


# try to find custom qemu if not installed in usual place
[ "$QEMU" ] || {
	PKG_MGR=`which rpm || which dpkg`
	case $PKG_MGR in
	*rpm)	QEMU_PKG=`rpm -q -l qemu-kvm` ;;
	*dpkg)	QEMU_PKG=`dpkg -L qemu-kvm`	;;
	*)	die "Couldn't locate package manager at `uname -a`"	;;
	esac

	QEMU=`echo "$QEMU_PKG" | grep 'bin/\(qemu\|kvm\|qemu-kvm\)$'`

	[ "$QEMU" ] || die "$QEMU_PKG - cannot find qemu/kvm executable"

	QEMU_SHARE=`echo "$QEMU" | sed 's#bin/.*#share#'`
}


call_gdb ( ) {
	[ "$UNATTENDED" ] || {
		echo "GAME OVER. Press Enter to start GDB..."
		read n
	}
	port="${1:-$GDB_PORT}"
	shift
	pid="$1"
	shift
	cd $PHANTOM_HOME
	echo "

FATAL! Phantom stopped (panic)"
	echo "
set confirm off
set pagination off
symbol-file oldtree/kernel/phantom/phantom.pe
dir oldtree/kernel/phantom
dir phantom/vm
dir phantom/libc
dir phantom/libc/ia32
dir phantom/dev
dir phantom/libphantom
dir phantom/newos
dir phantom/threads

target remote localhost:$port

bt full
quit
" > .gdbinit
	gdb

	[ "$1" ] && echo "$*"
	[ "$pid" ] && kill -9 $pid
	exit 1
}

# update data BEFORE checking for stalled copies
GRUB_MENU=tftp/tftp/menu.lst

# now proceed to test run
LOGFILE=serial0.log
cd $TEST_DIR
cp $TFTP_PATH/phantom tftp/

for module in classes pmod_test pmod_tcpdemo
do
	[ -s tftp/$module ] || {
		cp $TFTP_PATH/$module tftp/
		continue
	}

	[ tftp/$module -ot $TFTP_PATH/$module ] || continue

	echo "$module is renewed"
	cp $TFTP_PATH/$module tftp/
done

rm -f $LOGFILE

[ "$DISPLAY" ] && GRAPH="-vga cirrus" || GRAPH=-nographic

QEMU_OPTS="-L $QEMU_SHARE $GRAPH \
	-M pc -smp 4 $GDB_OPTS -boot a -no-reboot \
	-net nic,model=ne2k_pci -net user \
	-parallel file:lpt_01.log \
	-serial file:serial0.log \
	-tftp tftp \
	-no-fd-bootchk \
	-fda img/grubfloppy.img \
	-hda snapcopy.img \
	-hdb $DISK_IMG \
	-drive file=vio.img,if=virtio,format=raw \
	-usb -usbdevice mouse \
	-soundhw all"
#	-net dump,file=net.dmp \
#	-net nic,model=ne2k_isa -M isapc \

echo "color yellow/blue yellow/magenta
timeout=3

title=phantom ALL TESTS
kernel=(nd)/phantom -d=20 $UNATTENDED -- -test all
module=(nd)/classes
module=(nd)/pmod_test
boot 
" > $GRUB_MENU

dd if=/dev/zero of=snapcopy.img bs=4096 skip=1 count=1024 2> /dev/null
dd if=/dev/zero of=vio.img bs=4096 skip=1 count=1024 2> /dev/null

# take working copy of the Phantom disk
[ -s $DISK_IMG ] || cp ../../oldtree/run_test/$DISK_IMG .

$QEMU $QEMU_OPTS &
QEMU_PID=$!

# wait for Phantom to start
ELAPSED=2
sleep 2

while [ $ELAPSED -lt $WAIT_LAUNCH ]
do
	[ -s $LOGFILE ] && break

	sleep 2
	kill -0 $QEMU_PID || break
	ELAPSED=`expr $ELAPSED + 2`
done

[ -s $LOGFILE ] || {
	ELAPSED=$PANIC_AFTER
	LOG_MESSAGE="$LOGFILE is empty"
}

while [ $ELAPSED -lt $PANIC_AFTER ]
do

	sleep 2
	kill -0 $QEMU_PID || break
	ELAPSED=`expr $ELAPSED + 2`

	tail -1 $LOGFILE | grep -q '^Press any' && \
		call_gdb $GDB_PORT $QEMU_PID "Test run failed"

	grep -q '^\(\. \)\?Panic' $LOGFILE && {
		sleep 15	# allow panic to finish properly
		break
	}
done

# check if finished in time
[ $ELAPSED -lt $PANIC_AFTER ] || {
	echo "

FATAL! Phantom stalled: ${LOG_MESSAGE:-no activity after $PANIC_AFTER seconds}"
	kill $QEMU_PID
}

if [ "$CRONMODE" ]
then
	cat $LOGFILE | sed 's/^[[^m]*m//g;s/^M//g' 	# submit all details into the CI log, cutting off ESC-codes
else
	grep -q 'TEST FAILED' $LOGFILE && {
		cp $LOGFILE test.log
		#preserve_log test.log
	}
fi

# perform final checks
grep -B 10 'Panic\|[^e]fault\|^EIP\|^- \|Stack:\|^T[0-9 ]' $LOGFILE && die "Phantom test run failed!"
grep 'SVN' $LOGFILE || die "Phantom test run crashed!"
# show test summary in output
grep '[Ff][Aa][Ii][Ll]\|TEST\|SKIP' $LOGFILE
grep 'FINISHED\|done, reboot' $LOGFILE || die "Phantom test run error!"
