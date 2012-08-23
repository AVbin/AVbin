/* avbin_dump.c
 * Copyright 2012 AVbin Team
 *
 * This file is part of AVbin.
 *
 * AVbin is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of
 * the License, or (at your option) any later version.
 *
 * AVbin is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

/* Example use of AVbin.
 *
 * Prints out AVbin details, then stream details, then exits.
 *
 * TODO: Clean up, comment.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include <avbin.h>

int main(int argc, char** argv)
{
    if (avbin_init()) 
    {
        printf("Fatal: Couldn't initialize AVbin");
        exit(-1);
    }

    /* To store command-line flags */
    int verbose = 0;       /* -v, --verbose */
    int help = 0;          /* -h, --help */
    char * filename = "";  /* media file to inspect */

    /* Process command-line arguments */
    int i;
    for (i = 1; i < argc; i++)
    {
        if ( (strcmp(argv[i], "-v") == 0) || (strcmp(argv[i], "--verbose") == 0) )
            verbose = 1;
        else if ( (strcmp(argv[i], "-h") == 0) || (strcmp(argv[i], "--help") == 0) )
            help = 1;
        else if (strcmp(filename, "") == 0)
            filename = argv[i];
        else
        {
            printf("Invalid argument.  Try --help\n\n");
            exit(-3);
        }
    }

    /* Print help usage and exit, if that's what was selected */
    if (help)
    {
        printf("Usage: avbin_dump [options] [filename]\n\n  -h, --help     Print this help message.\n  -v, --verbose  Run through each packet in the media file and print out some info.\n\n");
        exit(0);
    }

    AVbinInfo *info =  avbin_get_info();

    printf("AVbin %s (feature version %d) built on %s\n  Repo: %s\n  Commit: %s\n\n",
           info->version_string,
           info->version,
           info->build_date,
           info->repo,
           info->commit);

    printf("Backend: %s %s\n  Repo: %s\n  Commit: %s\n\n",
           info->backend,
           info->backend_version_string,
           info->repo,
           info->backend_commit);

    if ( strcmp(filename, "") == 0 ) 
    {
        printf("If you specify a media file, we will print information about it, for example:\n./avbin_dump some_file.mp3\n");
        exit(-1);
    }

    AVbinFile* file = avbin_open_filename(filename);
    if (!file) 
    {
        printf("Unable to open file '%s'\n", filename);
        exit(-1);
    }

    AVbinFileInfo fileinfo;
    fileinfo.structure_size = sizeof(fileinfo);

    if (avbin_file_info(file, &fileinfo)) 
        exit(-2);

    printf("#streams %d\n",fileinfo.n_streams);
    printf("start time %" PRId64 "\n", fileinfo.start_time);
    printf("duration %lldus (%lld:%02lld:%02lld)\n",
        fileinfo.duration,
        fileinfo.duration / (1000000L * 60 * 60),
        (fileinfo.duration / (1000000L * 60)) % 60,
        (fileinfo.duration / 1000000L) % 60);

    printf("Title: %s\n", fileinfo.title);
    printf("Author: %s\n", fileinfo.author);
    printf("Copyright: %s\n", fileinfo.copyright);
    printf("Comment: %s\n", fileinfo.comment);
    printf("Album: %s\n", fileinfo.album);
    printf("Track: %d\n", fileinfo.track);
    printf("Year: %d\n", fileinfo.year);
    printf("Genre: %s\n", fileinfo.genre);

    AVbinStream* video_stream = NULL;
    AVbinStream* audio_stream = NULL;
    int video_stream_index = -1;
    int audio_stream_index = -1;
    int width, height;
    int have_frame_rate = avbin_have_feature("frame_rate");

    int stream_index;
    for (stream_index=0; stream_index<fileinfo.n_streams; stream_index++)
    {
        AVbinStreamInfo8 streaminfo;
        streaminfo.structure_size = sizeof(streaminfo);

        avbin_stream_info(file, stream_index, (AVbinStreamInfo *) &streaminfo);

        if (streaminfo.type == AVBIN_STREAM_TYPE_VIDEO)
        {
            printf("video stream at %d, height %d, width %d\n",stream_index,streaminfo.video.height,streaminfo.video.width);
            if (have_frame_rate)
                printf("frame rate %d / %d (approximately %.2f)\n", 
                       streaminfo.video.frame_rate_num, 
                       streaminfo.video.frame_rate_den,
                       streaminfo.video.frame_rate_num / (float)
                            streaminfo.video.frame_rate_den);
            width = streaminfo.video.width;
            height = streaminfo.video.height;
            video_stream_index = stream_index;
            video_stream = avbin_open_stream(file, stream_index);
        }
        if (streaminfo.type == AVBIN_STREAM_TYPE_AUDIO)
        {
            printf("audio stream at %d, rate %d, bits %d, chan %d\n",stream_index,streaminfo.audio.sample_rate,streaminfo.audio.sample_bits,streaminfo.audio.channels);
            audio_stream_index = stream_index;
            audio_stream = avbin_open_stream(file, stream_index);
        }
    }

    if (!verbose)
        exit(0);

    AVbinPacket packet;
    packet.structure_size = sizeof(packet);

    while (!avbin_read(file, &packet))
    {
        if (packet.stream_index == video_stream_index)
        {
            uint8_t* video_buffer = (uint8_t*) malloc(width*height*3);
            if (avbin_decode_video(video_stream, packet.data, packet.size,video_buffer)<=0) printf("could not read video packet\n");
            else printf("read video frame\n");

            // do something with video_buffer

            free(video_buffer);
        }
        if (packet.stream_index == audio_stream_index)
        {
            uint8_t audio_buffer[1024*1024];
            int bytesleft = sizeof(audio_buffer);
            int bytesout = bytesleft;
            int bytesread;
            uint8_t* audio_data = audio_buffer;
            while ((bytesread = avbin_decode_audio(audio_stream, packet.data, packet.size, audio_data, &bytesout)) > 0)
            {
                packet.data += bytesread;
                packet.size -= bytesread;
                audio_data += bytesout;
                bytesleft -= bytesout;
                bytesout = bytesleft;
            }

            int nrBytes = audio_data-audio_buffer;

            printf("[%" PRId64 "] read audio packet of size %d bytes\n", packet.timestamp, nrBytes);

            // do something with audio_buffer ... but don't free it since it is a local array
        }
    }

    if (video_stream) avbin_close_stream(video_stream);
    if (audio_stream) avbin_close_stream(audio_stream);

    avbin_close_file(file);
}
