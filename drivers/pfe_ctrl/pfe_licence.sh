#! /bin/sh
#
#  Copyright (C) 2007 Freescale Semiconductor, Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


if [ -z "$1" ]; then
	echo "Invalid PFE directory path"
	exit
fi
PFE_PATH=`pwd`/$1
if [ ! -d "$PFE_PATH" ]; then
	echo "PFE directory path does not exist : $PFE_PATH"
	exit
fi
pfe_files=$(find ${PFE_PATH} -type f)
#find  ${PFE_PATH} -type f > pfe_files.log
confidential_files_no=$(grep -nr "THIS FILE IS CONFIDENTIAL" ${PFE_PATH} | wc -l)
authorized_files_no=$(grep -nr "AUTHORIZED USE IS GOVERNED BY CONFIDENTIALITY AND LICENSE AGREEMENTS WITH FREESCALE SEMICONDUCTOR, INC." ${PFE_PATH} | wc -l)
unauthorized_files_no=$(grep -nr "UNAUTHORIZED COPIES AND USE ARE STRICTLY PROHIBITED AND MAY RESULT IN CRIMINAL AND/OR CIVIL PROSECUTION" ${PFE_PATH} | wc -l)
copyright_files_no=$(grep -nr "Copyright" ${PFE_PATH} --exclude="licencse.txt" --exclude="license_full.txt" | wc -l)
if [ -z "$confidential_files_no" -o -z "$authorized_files_no" -o -z "$unauthorized_files_no" -o -z "$copyright_files_no" ]; then
	echo "Invalid pattern search. Please check it"
	exit
fi
if [ $confidential_files_no -ne $authorized_files_no -o $confidential_files_no -ne $unauthorized_files_no -o $confidential_files_no -ne $copyright_files_no ]; then
  echo "Removed text pattern not matched properly. $confidential_files_no: $authorized_files_no : $unauthorized_files_no : $copyright_files_no . Please check the patterns"
  exit
fi
confidential_files=$(grep -nr "THIS FILE IS CONFIDENTIAL" ${PFE_PATH} | cut -d':' -f 1)
#grep -nr "THIS FILE IS CONFIDENTIAL" ${PFE_PATH} | cut -d':' -f 1 > confidential_files.log
#no_license_files=$(diff pfe_files_list pfe_conf_files_list --new-line-format="" --old-line-format="%L" --unchanged-line-format="")

for pfe_file in $pfe_files ; do
	matched=0
	for conf_file in $confidential_files ; do
		if [ $pfe_file = $conf_file ]; then
			matched=1
			break
		fi
	done
	if [ $matched -eq 0 ]; then
		no_license_files="$no_license_files $pfe_file"
	fi
done
#echo $no_license_files > no_license_files.log
DEBUG=""
for conf_file in $confidential_files ; do
	if [ `basename $conf_file` = "license.txt" -o `basename $conf_file` = "license_full.txt" ]; then
		continue
	fi
	sed -i '/THIS FILE IS CONFIDENTIAL/,+4 {d}'  $conf_file
	copy_right_line_no=$(grep -nr "Copyright" $conf_file | cut -d':' -f 1)
#	echo "copy_right_line_no=$copy_right_line_no file $conf_file"
	if [ -z "$copy_right_line_no" -o $copy_right_line_no -eq 0 ]; then
		echo "Invalid copy right line number of file : $conf_file. Please check it"
		exit
	fi
	if [ $copy_right_line_no -ge 4 ]; then
		echo "copy right line present after line no 4 in file : $conf_file. Please check it"
		exit
	fi
	$DEBUG head -n $copy_right_line_no $conf_file > ${conf_file}.tmp
	file_ext=$(basename $conf_file | cut -d'.' -f 2)
	if [ "$file_ext" = "Makefile" -o "$file_ext" = "makefile" -o "$file_ext" = "mk" ]; then
		cat ${PFE_PATH}/license.txt | sed -r  's/\*/#/g' >>${conf_file}.tmp
	else
		cat ${PFE_PATH}/license.txt >>${conf_file}.tmp
	fi
	tail_line_no=`expr $copy_right_line_no + 1`
#	echo "conf_file=$conf_file tail_line_no=$tail_line_no"
	$DEBUG tail -n +$tail_line_no $conf_file >>${conf_file}.tmp
	$DEBUG mv ${conf_file}.tmp ${conf_file}
done
for no_license_file in $no_license_files ; do
	if [ `basename $no_license_file` = "license.txt" -o `basename $no_license_file` = "license_full.txt" ]; then
		continue
	fi
	file_ext=$(basename $no_license_file | cut -d'.' -f 2)
	if [ "$file_ext" = "Makefile" -o "$file_ext" = "makefile" -o "$file_ext" = "mk" ]; then
		cat ${PFE_PATH}/license_full.txt | sed -r 's/\/\*/#/g' | sed -r 's/\*\//#/g' | sed -r  's/\*/#/g' >${no_license_file}.tmp
	else
		cat ${PFE_PATH}/license_full.txt >${no_license_file}.tmp
	fi
	cat $no_license_file >>${no_license_file}.tmp
	mv ${no_license_file}.tmp ${no_license_file}
done
