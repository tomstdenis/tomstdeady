#!/bin/bash

#sometimes I want to re-encode for low-spec playback
RECODE="-c:v libx264 -pix_fmt yuv420p -crf 23 -profile:v baseline -preset fast"
#otherwise just copy the video out
COPY="-c:v copy"
#which audio track to use
SRCAUD="eng"

if [ "$1" == "-recode" ]; then
       OP="${RECODE}"
       shift
else
       OP="${COPY}"
fi

#make a temp file safely
TMP=`mktemp`
#extract audio
ffmpeg -y -i "${1}" -map 0:a:m:language:${SRCAUD} -f s16le -c:a pcm_s16le -ar 48000 -ac 2 ${TMP}
#compress (to 35%)
tomsteady ${TMP} 35 | tee ${TMP}.log
#reinsert
ffmpeg -i "${1}" \
       -f s16le -ar 48000 -ac 2 -i ${TMP} \
       -map 0:v -map 1:a:0 \
	-max_interleave_delta 0 \
       ${OP} -c:a aac -b:a 128k -map 0:s:m:language:${SRCAUD}? -c:s copy -map_metadata 0 \
       "${2}"
#cleanup
rm -f ${TMP}
cat ${TMP}.log
rm -f ${TMP}.log
