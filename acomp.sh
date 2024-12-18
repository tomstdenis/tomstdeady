#!/bin/bash
#make a temp file safely
TMP=`mktemp`
#extract audio
ffmpeg -y -i "${1}" -map 0:a:0 -f s16le -c:a pcm_s16le -ar 48000 -ac 2 ${TMP}
#compress (to 35%)
./tomsteady ${TMP} 35 | tee ${TMP}.log
#reinsert
ffmpeg -i "${1}" \
       -f s16le -ar 48000 -ac 2 -i ${TMP} \
       -map 0:v -map 1:a:0 \
       -c:v copy -c:a aac -b:a 128k -map 0:s:0? -c:s copy -map_metadata 0 \
       "${2}"
#cleanup
rm -f ${TMP}
cat ${TMP}.log
rm -f ${TMP}.log

