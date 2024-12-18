// reincarnation of TomSteady
// assumes s16le format 48kHz PCM files
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>

// 100ms
#define BLKSIZE 4800

// default 30% (of 32768 which is the max abs 16-bit value)
int32_t TARGET = 9830;

// Max sample before clip protection engages
#define THRESHOLD 32000

// Max amplification
#define MAXVOL (65536L * 4)

struct state {
    unsigned curbuf;
    int32_t curstep, curvol; // current volume multiplier in 16.16
    struct {
        unsigned up, down, redo;
    } stats;
};

void remap_buffer(struct state *st, int16_t *buf, unsigned size)
{
    int32_t m, r, t;
    int16_t tmp[2*BLKSIZE];
    unsigned x, y;

    // now remap the buffer
    // we redo the mapping if we end up with any samples over THRESHOLD
    // so as to prevent clipping
redo:
    for (m = x = 0; x < 2; x++) {
        for (y = 0; y < size; y++) {
            tmp[y * 2 + x] = r = (((int32_t)buf[y*2 + x] * st->curvol)) >> 16;
            if (r < 0)
                r = -r;
            if (r > m)
                m = r;
            if (r >= THRESHOLD) {
                // way too loud go back a bunch
                st->curvol -= 16;
                if (st->curvol < 1) {
                    st->curvol = 1;
                }
                ++(st->stats.redo);
                goto redo;
            }
        }
    }

// change volume
    if (m < TARGET) {
        ++(st->stats.up);
        // we're below the target so increase the amplification
        // but cap at 2x volume
        if (st->curstep < 0)
            st->curstep = 1;
        st->curvol += st->curstep;
        if (st->curvol >= MAXVOL) {
            st->curvol = MAXVOL-1;
        }
        // we ramp up the increments over time but want
        // to cap them so it doesn't change volume too radically
        st->curstep += 16;
        if (st->curstep > 1024)
            st->curstep = 1024;
    } else if (m > TARGET) {
        ++(st->stats.down);
        if (st->curstep > 0)
            st->curstep = -1;
        st->curvol += st->curstep;
        if (st->curvol < 1) {
            st->curvol = 1;
        }
        // we ramp up the increments over time but want
        // to cap them so it doesn't change volume too radically
        st->curstep -= 16;
        if (st->curstep < -1024)
            st->curstep = -1024;
    }

    // finished so let's copy out
    for (x = 0; x < size * 2; x++) {
        buf[x] = tmp[x];
    }

    ++(st->curbuf);
}

int main(int argc, char **argv)
{
    int fd, size;
    int16_t blk[BLKSIZE*2];
    struct state st;

    if (argc <= 1) {
        printf("%s: filename [TARGET_PERCENT]\n\nNote: File must be in stereo s16le PCM format\n", argv[0]);
        return 0;
    }

    if (argc > 2) {
        TARGET = ((int32_t)atoi(argv[2]) * 32768) / 100;
    }

    printf("Tomsteady:\n\tRemapping to a target of %d\n", (int)TARGET);
    memset(&st, 0, sizeof st);
    st.curvol = TARGET;
    st.curstep = 1;
    fd = open(argv[1], O_RDWR);
    if (fd < 0) {
        perror("Could not open PCM file");
        return -1;
    }
    do {
        size = read(fd, blk, BLKSIZE * 4);
        remap_buffer(&st, blk, size/4);
        lseek(fd, -size, SEEK_CUR);
        write(fd, blk, size);
    } while (size == BLKSIZE * 4);
    close(fd);
    printf("\tstats: Up %u, Down %u, Redo %u\n", st.stats.up, st.stats.down, st.stats.redo);
}