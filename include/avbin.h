/* avbin.h
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

/** 
 * @file
 * AVbin functions and structures.
 */

/**
 * @mainpage
 *
 * To open a file and prepare it for decoding, the general procedure is
 *   -# (Optionally) call avbin_have_feature() to check which features are
        available.  This is only needed if your application may be deployed in
        environments with older versions of AVbin than you are developing to.
 *   -# Initialize AVbin by calling avbin_init_options()
 *   -# Open a sound or video file using avbin_open_filename()
 *   -# Retrieve details of the file using avbin_file_info().  The resulting
 *      _AVbinFileInfo structure includes details such as:
 *        - Start time and duration
 *        - Number of audio and video streams
 *        - Metadata such as title, artist, etc.
 *   -# Examine details of each stream using avbin_stream_info(), passing
 *      in each stream index as an integer from 0 to n_streams.  For
 *      video streams, the _AVbinStreamInfo structure includes
 *        - Video width and height, in pixels
 *        - Pixel aspect ratio, expressed as a fraction
 *      For audio streams, the structure includes
 *        - Sample rate, in Hz
 *        - Bits per sample
 *        - Channels (monoaural, stereo, or multichannel surround)
 *   -# For each stream you intend to decode, call avbin_open_stream().
 *   
 * When all information has been determined and the streams are open, you can
 * proceed to read and decode the file:
 *   -# Call avbin_read() to read a packet of data from the file.
 *   -# Examine the resulting _AVbinPacket structure for the stream_index,
 *      which indicates how the packet should be decoded.  If the stream is
 *      not one that you have opened, you can discard the packet and continue
 *      with step 1 again.
 *   -# To decode an audio packet, repeatedly pass the data within the packet
 *      to avbin_decode_audio(), until there is no data left to consume or an
 *      error is returned.
 *   -# To decode a video packet, pass the data within the packet to
 *      avbin_decode_video(), which will decode a single image in RGB format.
 *   -# Synchronise audio and video data by observing the
 *      _AVbinPacket::timestamp member.
 *
 * When decoding is complete, call avbin_close_stream() on each stream and
 * avbin_close_file() on the open file.
 */

/**
 * This stuff is required to use avbin.h in C++ programs
 * (yes, compilers are unable to distinguish C code from C++)
 */
#ifdef __cplusplus
extern "C" {
#endif

#ifndef AVBIN_H
#define AVBIN_H

#include <stdint.h>

/** 
 * Error-checked function result.
 */
typedef enum _AVbinResult {
    AVBIN_RESULT_ERROR = -1,
    AVBIN_RESULT_OK = 0
} AVbinResult;

/**
 * Type of a stream; currently only video and audio streams are supported.
 */
typedef enum _AVbinStreamType {
    AVBIN_STREAM_TYPE_UNKNOWN = 0,
    AVBIN_STREAM_TYPE_VIDEO = 1,
    AVBIN_STREAM_TYPE_AUDIO = 2
} AVbinStreamType;

/**
 * The sample format for audio data.
 */
typedef enum _AVbinSampleFormat {
    /** Unsigned byte */
    AVBIN_SAMPLE_FORMAT_U8 = 0,
    /** Signed 16-bit integer */
    AVBIN_SAMPLE_FORMAT_S16 = 1,
    /** Signed 24-bit integer 
     *
     *  @deprecated This format is not used.
     *  */
    AVBIN_SAMPLE_FORMAT_S24 = 2,
    /** Signed 32-bit integer */
    AVBIN_SAMPLE_FORMAT_S32 = 3,
    /** 32-bit IEEE floating-point */
    AVBIN_SAMPLE_FORMAT_FLOAT = 4
} AVbinSampleFormat;

/**
 * Threshold of logging verbosity.
 */
typedef enum _AVbinLogLevel {
    AVBIN_LOG_QUIET = -8,
    AVBIN_LOG_PANIC = 0,
    AVBIN_LOG_FATAL = 8,
    AVBIN_LOG_ERROR = 16,
    AVBIN_LOG_WARNING = 24,
    AVBIN_LOG_INFO = 32,
    AVBIN_LOG_VERBOSE = 40,
    AVBIN_LOG_DEBUG = 48
} AVbinLogLevel;

/**
 * Opaque open file handle.
 */
typedef struct _AVbinFile AVbinFile;

/**
 * Opaque open stream handle.
 */
typedef struct _AVbinStream AVbinStream;

/**
 * Point in time, or a time range; given in microseconds.
 */
typedef int64_t AVbinTimestamp;

/**
 * File details.  The info struct is filled in by avbin_get_file_info.
 */
typedef struct _AVbinFileInfo {
    /** 
     * Size of this structure, in bytes.  This must be filled in by the
     * application before passing to AVbin.
     */ 
    size_t structure_size;

    /**
     * Number of streams contained in the file.
     */
    int32_t n_streams;

    /**
     * Starting time of all streams.
     */
    AVbinTimestamp start_time;
    
    /**
     * Duration of the file.  Does not include the time given in start_time.
     */
    AVbinTimestamp duration;

    /** 
     * @name Metadata fields
     *
     * File metadata.
     *
     * Strings are NUL-terminated and may be omitted (the first character
     * NUL) if the file does not contain appropriate information.  The
     * encoding of the strings is unspecified.
     */
    /*@{*/
    char title[512];
    char author[512];
    char copyright[512];
    char comment[512];
    char album[512];
    int32_t year;
    int32_t track;
    char genre[32];
    /*@}*/
} AVbinFileInfo;

/**
 * Stream details.
 * 
 * A stream is a single audio track or video.  Most audio files contain one
 * audio stream.  Most video files contain one audio stream and one video
 * stream.  More than one audio stream may indicate the presence of multiple
 * languages which can be selected (however at this time AVbin does not
 * provide language information).
 */
typedef struct _AVbinStreamInfo {
    /** 
     * Size of this structure, in bytes.  This must be filled in by the
     * application before passing to AVbin.
     */ 
    size_t structure_size;

    /**
     * The type of stream; either audio or video.
     */
    AVbinStreamType type;

    union {
        struct {
            /**
             * Width of the video image, in pixels.  This is the width
             * of actual video data, and is not necessarily the size the
             * video is to be displayed at (see sample_aspect_num).
             */
            uint32_t width;

            /**
             * Height of the video image, in pixels.
             */
            uint32_t height;

            /**
             * Aspect-ratio of each pixel.  The aspect is given by dividing
             * sample_aspect_num by sample_aspect_den.
             */
            uint32_t sample_aspect_num;
            uint32_t sample_aspect_den;

            /** Frame rate, in frames per second.  The frame rate is given by
             * dividing frame_rate_num by frame_rate_den.
             *
             * @version Version 8.  Requires frame_rate feature. 
             */
            uint32_t frame_rate_num;
            uint32_t frame_rate_den;
        } video;

        struct {
            /**
             * Data type of audio samples.
             */
            AVbinSampleFormat sample_format;

            /**
             * Number of samples per second, in Hz.
             */
            uint32_t sample_rate;

            /**
             * Number of bits per sample; typically 8 or 16.
             */
            uint32_t sample_bits;

            /**
             * Number of interleaved audio channels.  Typically 1 for
             * monoaural, 2 for stereo.  Higher channel numbers are used for
             * surround sound, however AVbin does not currently provide a way
             * to access the arrangement of these channels.
             */
            uint32_t channels;
        } audio;
    };
} AVbinStreamInfo;

/**
 * Stream details, version 8.
 * 
 * A stream is a single audio track or video.  Most audio files contain one
 * audio stream.  Most video files contain one audio stream and one video
 * stream.  More than one audio stream may indicate the presence of multiple
 * languages which can be selected (however at this time AVbin does not
 * provide language information).
 */
typedef struct _AVbinStreamInfo8 {
    /** 
     * Size of this structure, in bytes.  This must be filled in by the
     * application before passing to AVbin.
     */ 
    size_t structure_size;

    /**
     * The type of stream; either audio or video.
     */
    AVbinStreamType type;

    union {
        struct {
            /**
             * Width of the video image, in pixels.  This is the width
             * of actual video data, and is not necessarily the size the
             * video is to be displayed at (see sample_aspect_num).
             */
            uint32_t width;

            /**
             * Height of the video image, in pixels.
             */
            uint32_t height;

            /**
             * Aspect-ratio of each pixel.  The aspect is given by dividing
             * sample_aspect_num by sample_aspect_den.
             */
            uint32_t sample_aspect_num;
            uint32_t sample_aspect_den;

            /** Frame rate, in frames per second.  The frame rate is given by
             * dividing frame_rate_num by frame_rate_den.
             *
             * @version Version 8.  Requires frame_rate extension. 
             */
            uint32_t frame_rate_num;
            uint32_t frame_rate_den;
        } video;

        struct {
            /**
             * Data type of audio samples.
             */
            AVbinSampleFormat sample_format;

            /**
             * Number of samples per second, in Hz.
             */
            uint32_t sample_rate;

            /**
             * Number of bits per sample; typically 8 or 16.
             */
            uint32_t sample_bits;

            /**
             * Number of interleaved audio channels.  Typically 1 for
             * monoaural, 2 for stereo.  Higher channel numbers are used for
             * surround sound, however AVbin does not currently provide a way
             * to access the arrangement of these channels.
             */
            uint32_t channels;
        } audio;
    };
} AVbinStreamInfo8;

/**
 * A single packet of stream data.
 *
 * The structure size must be initialised before passing to avbin_read.  The
 * data will point to a block of memory allocated by AVbin -- you must not
 * free it.  The data will be valid until the next time you call avbin_read,
 * or until the file is closed.
 */
typedef struct _AVbinPacket {
    /**
     * Size of this structure, in bytes.  This must be filled in by the
     * application before passing to AVbin.
     */ 
    size_t structure_size;

    /**
     * The time at which this packet is to be played.  This can be used
     * to synchronise audio and video data.
     */
    AVbinTimestamp timestamp;

    /**
     * The stream this packet contains data for.
     */
    uint32_t stream_index;

    uint8_t *data;
    size_t size;
} AVbinPacket;


/**
 * Information about the AVbin library.  See avbin_get_info()
 */
typedef struct _AVbinInfo {
    /**
     * Size of this structure, in bytes.  This will be filled in for you by
     * avbin_get_info() so that you can determine which version of this struct
     * you have received.  For example:
     *   AVbinInfo * info = avbin_get_info()
     *   if (info->structure_size == sizeof(AVbinInfo)
     *       // You are safe to access the members of an AVbinInfo typedef...
     */
    size_t structure_size;

    /**
     * AVbin version as an integer.  This value is the same as returned by
     * the avbin_get_version() function.  Consider using version_string instead.
     */
    int version;

    /**
     * AVbin version string, including pre-release information, i.e. "10-beta1".
     */
    char *version_string;

    /**
     * When the library was built, in strftime format "%Y-%m-%d %H:%M:%S %z"
     */
    char *build_date;

    /**
     * URL to the AVbin repository used.
     */
    char *repo;

    /**
     * The commit of the AVbin repository used.
     */
    char *commit;

    /**
     * Which backend we are using: "libav" or "ffmpeg"
     */
    char *backend;

    /**
     * The version string of the most recent tag of the backend.  Note: There
     * may be custom patches *on top* of the backend version.
     */
    char *backend_version_string;

    /**
     * URL to the backend repository used for this release.
     */
    char *backend_repo;

    /**
     * The commit hash of the backend repo used for the backend.
     */
    char *backend_commit;
} AVbinInfo;


/**
 * Initialization Options
 */
typedef struct _AVbinOptions {
    /**
     * Size of this structure, in bytes.  This must be filled in by the
     * application before passing to AVbin.
     */
    size_t structure_size;

    /**
     * Number of threads to attempt to use.  Using the recommended thread_count of
     * 0 means try to detect the number of CPU cores and set threads to
     * (num cores + 1).  A thread_count of 1 or a negative number means single
     * threaded.  Any other number will result in an attempt to set that many threads.
     */
    int32_t thread_count;
} AVbinOptions;


/**
 * Callback for log information.
 *
 * @param module  The name of the module where this message originated
 * @param level   The log verbosity level of this message
 * @param message The formatted message.  The message may or may not contain
 *                newline characters.
 */
typedef void (*AVbinLogCallback)(const char *module, 
                                 AVbinLogLevel level, 
                                 const char *message);

/** 
 * @name Information about AVbin
 */

/**
 * Get the linked version of AVbin.
 *
 * Version numbers are always integer, there are no "minor" or "patch"
 * revisions.  All AVbin versions are backward and forward compatible, modulo
 * the required feature set.
 */
int32_t avbin_get_version();


/**
 * Get information about the linked version of AVbin.
 *
 * See the AVbinInfo definition.
 */
AVbinInfo *avbin_get_info();


/**
 * Get the SVN revision of FFmpeg.
 *
 * This is built into AVbin as it is built.
 *
 * DEPRECATED: Use avbin_get_libav_commit or avbin_get_libav_version instead.
 *             This always returns 0 now that we use Libav from Git.  This
 *             function will be removed in AVbin 12.
 */
int32_t avbin_get_ffmpeg_revision() __attribute__((deprecated));


/**
 * Get the minimum audio buffer size, in bytes.
 */
size_t avbin_get_audio_buffer_size();

/**
 * Determine if AVbin includes a requested feature.
 *
 * When future versions of AVbin include more functionality, that
 * functionality can be tested for by calling this function.  The following
 * features can be tested for:
 *  - "frame_rate" // AVbinStreamInfo8
 *  - "options"    // avbin_init_options(), AVbinOptions (multi-threading)
 *  - "info"       // avbin_get_info(), AVbinInfo
 *
 * @retval 1 The feature is present
 * @retval 0 The feature is not present
 */
int32_t avbin_have_feature(const char *feature);
/*@}*/

/**
 * @name Global AVbin functions
 */

/**
 * One of the avbin_init* functions must be called before opening a file to 
 * initialize AVbin.  Check the return value for success before continuing.
 */

/**
 * Initialize AVbin with basic features.  Consider instead avbin_init_options()
 */
AVbinResult avbin_init();

/**
 * Initialize AVbin with options.
 *
 * @param options  If NULL, use defaults.  Otherwise create and populate an
 *                 instance of AVbinOptions to supply.
 */
AVbinResult avbin_init_options(AVbinOptions * options);

/**
 * Set the log level verbosity.
 */
AVbinResult avbin_set_log_level(AVbinLogLevel level);

/**
 * Set a custom log callback.  By default, log messages are printed to
 * standard error.  Providing a NULL callback restores this default handler.
 */
AVbinResult avbin_set_log_callback(AVbinLogCallback callback);
/*@}*/

/**
 * @name File handling functions
 */

/**
 * Open a media file given its filename.
 *
 * @retval NULL if the file could not be opened, or is not of a recognised
 *              file format.
 */
AVbinFile *avbin_open_filename(const char *filename);
AVbinFile *avbin_open_filename_with_format(const char *filename, char* format);

/**
 * Close a media file.
 */
void avbin_close_file(AVbinFile *file);

/**
 * Seek to a timestamp within a file.
 *
 * For video files, the first keyframe before the requested timestamp will be
 * seeked to.  For audio files, the first audio packet before the requested
 * timestamp is used.
 */
AVbinResult avbin_seek_file(AVbinFile *file, AVbinTimestamp timestamp);

/**
 * Get information about the opened file.
 *
 * The info struct must be allocated by the application and have its
 * structure_size member filled in correctly.  On return, the structure
 * will be filled with file details.
 */
AVbinResult avbin_file_info(AVbinFile *file,
                            AVbinFileInfo *info);
/*@}*/

/**
 * @name Stream functions
 */

/**
 * Get information about a stream within the file.
 *
 * The info struct must be allocated by the application and have its
 * structure_size member filled in correctly.  On return, the structure
 * will be filled with stream details.
 *
 * Ensure that stream_index is less than n_streams given in the file info.
 *
 * @param[in]  file          The file to examine
 * @param[in]  stream_index  The number of the stream within the file
 * @param[out] info          Returned stream information
 */
AVbinResult avbin_stream_info(AVbinFile *file, int32_t stream_index,
                              AVbinStreamInfo *info);

/**
 * Open a stream for decoding.
 *
 * If you intend to decode audio or video from a file, you must open the
 * stream first.  The returned opaque handle should be passed to the relevant
 * decode function when a packet for that stream is read.
 */
AVbinStream *avbin_open_stream(AVbinFile *file, int32_t stream_index);

/**
 * Close a file stream.
 */
void avbin_close_stream(AVbinStream *stream);
/*@}*/

/** 
 * @name Reading and decoding functions
 */

/**
 * Read a packet from the file.
 *
 * The packet struct must be allocated by the application and have its
 * structure_size member filled in correctly.  On return, the structure
 * will be filled with a packet of data.  The actual data pointer within
 * the packet must not be freed, and is valid until the next call to
 * avbin_read.
 * 
 * Applications should examine the packet's stream index to match it with
 * an appropriate open stream handle, or discard it if none match.  The packet
 * data can then be passed to the relevant decode function.
 */
AVbinResult avbin_read(AVbinFile *file, AVbinPacket *packet);

/**
 * Decode some audio data.
 *
 * You must ensure that data_out is at least as big as the minimum audio
 * buffer size (see avbin_get_audio_buffer_size()).
 *
 * @param[in]  stream    The stream to decode.
 * @param[in]  data_in   Incoming data, as read from a packet
 * @param[in]  size_in   Size of data_in, in bytes
 * @param[out] data_out  Decoded audio data buffer, provided by application
 * @param[out] size_out  Number of bytes of data_out used.
 *
 * @return the number of bytes of data_in actually used.  You should call
 *         this function repeatedly as long as the return value is greater
 *         than 0.
 *
 * @retval -1 if there was an error
 */ 
int32_t avbin_decode_audio(AVbinStream *stream,
                       uint8_t *data_in, size_t size_in,
                       uint8_t *data_out, int *size_out);

/**
 * Decode a video frame image.
 *
 * The size of data_out must be large enough to hold the entire image.
 * This is width * height * 3 (images are always in 8-bit RGB format).
 *
 * @param[in]  stream   The stream to decode.
 * @param[in]  data_in  Incoming data, as read from a packet
 * @param[in]  size_in  Size of data_in, in bytes
 * @param[out] data_out Decoded image data.
 *
 * @return the number of bytes of data_in actually used.  Any remaining bytes
 *         can be discarded.
 *
 * @retval -1 if there was an error
 */
int32_t avbin_decode_video(AVbinStream *stream,
                       uint8_t *data_in, size_t size_in,
                       uint8_t *data_out);

/*@}*/

#endif

#ifdef __cplusplus
}
#endif
