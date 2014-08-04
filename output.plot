pmtitle = `head -n1 /dev/stdin | awk '{print $2,$3}'`
set title pmtitle
set xlabel "Seconds"
set ylabel "Mem Usage (kB)"
set format y "%.0f"
set key outside
set style line 1 lt 2 lc rgb "red" lw 3
plot "/dev/stdin" using 1:2 title 'VmPeak' with points pt 7 lc 2, \
"/dev/stdin" using 1:3 title 'VmSize' with line lc 2, \
"/dev/stdin" using 1:4 title 'VmHWM' with points pt 2 lc 3, \
"/dev/stdin" using 1:5 title 'VmRSS' with line lc 3 
