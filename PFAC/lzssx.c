/***************************************************************************
*                             BITFILE.C
***************************************************************************/

/***************************************************************************
*                             INCLUDED FILES
***************************************************************************/
#include <stdlib.h>
#include <errno.h>
#include "bitfile.h"

/***************************************************************************
*                            TYPE DEFINITIONS
***************************************************************************/

/***************************************************************************
* type to point to the kind of functions that put/get bits from/to numerical
* data types (short, int, long, ...)
* parameters: file pointer, data structure, number of bits, sizeof data.
***************************************************************************/
typedef int (*num_func_t)(bit_file_t*, void*, const unsigned int, const size_t);

struct bit_file_t
{
    FILE *fp;                   /* file pointer used by stdio functions */
    unsigned char bitBuffer;    /* bits waiting to be read/written */
    unsigned char bitCount;     /* number of bits in bitBuffer */
    num_func_t PutBitsNumFunc;  /* endian specific BitFilePutBitsNum */
    num_func_t GetBitsNumFunc;  /* endian specific BitFileGetBitsNum */
    BF_MODES mode;              /* open for read, write, or append */
};

typedef enum
{
    BF_UNKNOWN_ENDIAN,
    BF_LITTLE_ENDIAN,
    BF_BIG_ENDIAN
} endian_t;

/* union used to test for endianess */
typedef union
{
    unsigned long word;
    unsigned char bytes[sizeof(unsigned long)];
} endian_test_t;

/***************************************************************************
*                               PROTOTYPES
***************************************************************************/
static endian_t DetermineEndianess(void);

static int BitFilePutBitsLE(bit_file_t *stream, void *bits,
    const unsigned int count, const size_t size);
static int BitFilePutBitsBE(bit_file_t *stream, void *bits,
    const unsigned int count, const size_t size);

static int BitFileGetBitsLE(bit_file_t *stream, void *bits,
    const unsigned int count, const size_t size);
static int BitFileGetBitsBE(bit_file_t *stream, void *bits,
    const unsigned int count, const size_t size);
static int BitFileNotSupported(bit_file_t *stream, void *bits,
    const unsigned int count, const size_t size);

/***************************************************************************
*                                FUNCTIONS
***************************************************************************/

/***************************************************************************
*   Function   : BitFileOpen
*   Description: This function opens a bit file for reading, writing,
*                or appending.  If successful, a bit_file_t data
*                structure will be allocated and a pointer to the
*                structure will be returned.
*   Parameters : fileName - NULL terminated string containing the name of
*                           the file to be opened.
*                mode - The mode of the file to be opened
*   Effects    : The specified file will be opened and file structure will
*                be allocated.
*   Returned   : Pointer to the bit_file_t structure for the bit file
*                opened, or NULL on failure.  errno will be set for all
*                failure cases.
***************************************************************************/
bit_file_t *BitFileOpen(const char *fileName, const BF_MODES mode)
{
    const char modes[3][3] = {"rb", "wb", "ab"};    /* binary modes for fopen */
    bit_file_t *bf;

    bf = (bit_file_t *)malloc(sizeof(bit_file_t));

    if (bf == NULL)
    {
        /* malloc failed */
        errno = ENOMEM;
    }
    else
    {
        bf->fp = fopen(fileName, modes[mode]);

        if (bf->fp == NULL)
        {
            /* fopen failed */
            free(bf);
            bf = NULL;
        }
        else
        {
            /* fopen succeeded fill in remaining bf data */
            bf->bitBuffer = 0;
            bf->bitCount = 0;
            bf->mode = mode;

            switch (DetermineEndianess())
            {
                case BF_LITTLE_ENDIAN:
                    bf->PutBitsNumFunc = &BitFilePutBitsLE;
                    bf->GetBitsNumFunc = &BitFileGetBitsLE;
                    break;

                case BF_BIG_ENDIAN:
                    bf->PutBitsNumFunc = &BitFilePutBitsBE;
                    bf->GetBitsNumFunc = &BitFileGetBitsBE;
                    break;

                case BF_UNKNOWN_ENDIAN:
                default:
                    bf->PutBitsNumFunc = BitFileNotSupported;
                    bf->GetBitsNumFunc = BitFileNotSupported;
                    break;
            }

            /***************************************************************
            * TO DO: Consider using the last byte in a file to indicate
            * the number of bits in the previous byte that actually have
            * data.  If I do that, I'll need special handling of files
            * opened with a mode of BF_APPEND.
            ***************************************************************/
        }
    }

    return (bf);
}

/***************************************************************************
*   Function   : MakeBitFile
*   Description: This function naively wraps a standard file in a
*                bit_file_t structure.  ANSI-C doesn't support file status
*                functions commonly found in other C variants, so the
*                caller must be passed as a parameter.
*   Parameters : stream - pointer to the standard file being wrapped.
*                mode - The mode of the file being wrapped.
*   Effects    : A bit_file_t structure will be created for the stream
*                passed as a parameter.
*   Returned   : Pointer to the bit_file_t structure for the bit file
*                or NULL on failure.  errno will be set for all failure
*                cases.
***************************************************************************/
bit_file_t *MakeBitFile(FILE *stream, const BF_MODES mode)
{
    bit_file_t *bf;

    if (stream == NULL)
    {
        /* can't wrapper empty steam */
        errno = EBADF;
        bf = NULL;
    }
    else
    {
        bf = (bit_file_t *)malloc(sizeof(bit_file_t));

        if (bf == NULL)
        {
            /* malloc failed */
            errno = ENOMEM;
        }
        else
        {
            /* set structure data */
            bf->fp = stream;
            bf->bitBuffer = 0;
            bf->bitCount = 0;
            bf->mode = mode;

            switch (DetermineEndianess())
            {
                case BF_LITTLE_ENDIAN:
                    bf->PutBitsNumFunc = &BitFilePutBitsLE;
                    bf->GetBitsNumFunc = &BitFileGetBitsLE;
                    break;

                case BF_BIG_ENDIAN:
                    bf->PutBitsNumFunc = &BitFilePutBitsBE;
                    bf->GetBitsNumFunc = &BitFileGetBitsBE;
                    break;

                case BF_UNKNOWN_ENDIAN:
                default:
                    bf->PutBitsNumFunc = BitFileNotSupported;
                    bf->GetBitsNumFunc = BitFileNotSupported;
                    break;
            }
        }
    }

    return (bf);
}

/***************************************************************************
*   Function   : DetermineEndianess
*   Description: This function determines the endianess of the current
*                hardware architecture.  An unsigned long is set to 1.  If
*                the 1st byte of the unsigned long gets the 1, this is a
*                little endian machine.  If the last byte gets the 1, this
*                is a big endian machine.
*   Parameters : None
*   Effects    : None
*   Returned   : endian_t for current machine architecture
***************************************************************************/
static endian_t DetermineEndianess(void)
{
    endian_t endian;
    endian_test_t endianTest;

    endianTest.word = 1;

    if (endianTest.bytes[0] == 1)
    {
        /* LSB is 1st byte (little endian)*/
        endian = BF_LITTLE_ENDIAN;
    }
    else if (endianTest.bytes[sizeof(unsigned long) - 1] == 1)
    {
        /* LSB is last byte (big endian)*/
        endian = BF_BIG_ENDIAN;
    }
    else
    {
        endian = BF_UNKNOWN_ENDIAN;
    }

    return endian;
}

/***************************************************************************
*   Function   : BitFileClose
*   Description: This function closes a bit file and frees all associated
*                data.
*   Parameters : stream - pointer to bit file stream being closed
*   Effects    : The specified file will be closed and the file structure
*                will be freed.
*   Returned   : 0 for success or EOF for failure.
***************************************************************************/
int BitFileClose(bit_file_t *stream)
{
    int returnValue = 0;

    if (stream == NULL)
    {
        return(EOF);
    }

    if ((stream->mode == BF_WRITE) || (stream->mode == BF_APPEND))
    {
        /* write out any unwritten bits */
        if (stream->bitCount != 0)
        {
            (stream->bitBuffer) <<= 8 - (stream->bitCount);
            fputc(stream->bitBuffer, stream->fp);   /* handle error? */
        }
    }

    /***********************************************************************
    *  TO DO: Consider writing an additional byte indicating the number of
    *  valid bits (bitCount) in the previous byte.
    ***********************************************************************/

    /* close file */
    returnValue = fclose(stream->fp);

    /* free memory allocated for bit file */
    free(stream);

    return(returnValue);
}

/***************************************************************************
*   Function   : BitFileToFILE
*   Description: This function flushes and frees the bitfile structure,
*                returning a pointer to a stdio file.
*   Parameters : stream - pointer to bit file stream being closed
*   Effects    : The specified bitfile will be made usable as a stdio
*                FILE.
*   Returned   : Pointer to FILE.  NULL for failure.
***************************************************************************/
FILE *BitFileToFILE(bit_file_t *stream)
{
    FILE *fp = NULL;

    if (stream == NULL)
    {
        return(NULL);
    }

    if ((stream->mode == BF_WRITE) || (stream->mode == BF_APPEND))
    {
        /* write out any unwritten bits */
        if (stream->bitCount != 0)
        {
            (stream->bitBuffer) <<= 8 - (stream->bitCount);
            fputc(stream->bitBuffer, stream->fp);   /* handle error? */
        }
    }

    /***********************************************************************
    *  TO DO: Consider writing an additional byte indicating the number of
    *  valid bits (bitCount) in the previous byte.
    ***********************************************************************/

    /* close file */
    fp = stream->fp;

    /* free memory allocated for bit file */
    free(stream);

    return(fp);
}

/***************************************************************************
*   Function   : BitFileByteAlign
*   Description: This function aligns the bitfile to the nearest byte.  For
*                output files, this means writing out the bit buffer with
*                extra bits set to 0.  For input files, this means flushing
*                the bit buffer.
*   Parameters : stream - pointer to bit file stream to align
*   Effects    : Flushes out the bit buffer.
*   Returned   : EOF if stream is NULL or write fails.  Writes return the
*                byte aligned contents of the bit buffer.  Reads returns
*                the unaligned contents of the bit buffer.
***************************************************************************/
int BitFileByteAlign(bit_file_t *stream)
{
    int returnValue;

    if (stream == NULL)
    {
        return(EOF);
    }

    returnValue = stream->bitBuffer;

    if ((stream->mode == BF_WRITE) || (stream->mode == BF_APPEND))
    {
        /* write out any unwritten bits */
        if (stream->bitCount != 0)
        {
            (stream->bitBuffer) <<= 8 - (stream->bitCount);
            fputc(stream->bitBuffer, stream->fp);   /* handle error? */
        }
    }

    stream->bitBuffer = 0;
    stream->bitCount = 0;

    return (returnValue);
}

/***************************************************************************
*   Function   : BitFileFlushOutput
*   Description: This function flushes the output bit buffer.  This means
*                left justifying any pending bits, and filling spare bits
*                with the fill value.
*   Parameters : stream - pointer to bit file stream to align
*                onesFill - non-zero if spare bits are filled with ones
*   Effects    : Flushes out the bit buffer, filling spare bits with ones
*                or zeros.
*   Returned   : EOF if stream is NULL or not writeable.  Otherwise, the
*                bit buffer value written. -1 if no data was written.
***************************************************************************/
int BitFileFlushOutput(bit_file_t *stream, const unsigned char onesFill)
{
    int returnValue;

    if (stream == NULL)
    {
        return(EOF);
    }

    returnValue = -1;

    /* write out any unwritten bits */
    if (stream->bitCount != 0)
    {
        stream->bitBuffer <<= (8 - stream->bitCount);

        if (onesFill)
        {
            stream->bitBuffer |= (0xFF >> stream->bitCount);
        }

        returnValue = fputc(stream->bitBuffer, stream->fp);
    }

    stream->bitBuffer = 0;
    stream->bitCount = 0;

    return (returnValue);
}

/***************************************************************************
*   Function   : BitFileGetChar
*   Description: This function returns the next byte from the file passed as
*                a parameter.
*   Parameters : stream - pointer to bit file stream to read from
*   Effects    : Reads next byte from file and updates buffer accordingly.
*   Returned   : EOF if a whole byte cannot be obtained.  Otherwise,
*                the character read.
***************************************************************************/
int BitFileGetChar(bit_file_t *stream)
{
    int returnValue;
    unsigned char tmp;

    if (stream == NULL)
    {
        return(EOF);
    }

    returnValue = fgetc(stream->fp);

    if (stream->bitCount == 0)
    {
        /* we can just get byte from file */
        return returnValue;
    }

    /* we have some buffered bits to return too */
    if (returnValue != EOF)
    {
        /* figure out what to return */
        tmp = ((unsigned char)returnValue) >> (stream->bitCount);
        tmp |= ((stream->bitBuffer) << (8 - (stream->bitCount)));

        /* put remaining in buffer. count shouldn't change. */
        stream->bitBuffer = returnValue;

        returnValue = tmp;
    }

    return returnValue;
}

/***************************************************************************
*   Function   : BitFilePutChar
*   Description: This function writes the byte passed as a parameter to the
*                file passed a parameter.
*   Parameters : c - the character to be written
*                stream - pointer to bit file stream to write to
*   Effects    : Writes a byte to the file and updates buffer accordingly.
*   Returned   : On success, the character written, otherwise EOF.
***************************************************************************/
int BitFilePutChar(const int c, bit_file_t *stream)
{
    unsigned char tmp;

    if (stream == NULL)
    {
        return(EOF);
    }

    if (stream->bitCount == 0)
    {
        /* we can just put byte from file */
        return fputc(c, stream->fp);
    }

    /* figure out what to write */
    tmp = ((unsigned char)c) >> (stream->bitCount);
    tmp = tmp | ((stream->bitBuffer) << (8 - stream->bitCount));

    if (fputc(tmp, stream->fp) != EOF)
    {
        /* put remaining in buffer. count shouldn't change. */
        stream->bitBuffer = c;
    }
    else
    {
        return EOF;
    }

    return tmp;
}

/***************************************************************************
*   Function   : BitFileGetBit
*   Description: This function returns the next bit from the file passed as
*                a parameter.  The bit value returned is the msb in the
*                bit buffer.
*   Parameters : stream - pointer to bit file stream to read from
*   Effects    : Reads next bit from bit buffer.  If the buffer is empty,
*                a new byte will be read from the file.
*   Returned   : 0 if bit == 0, 1 if bit == 1, and EOF if operation fails.
***************************************************************************/
int BitFileGetBit(bit_file_t *stream)
{
    int returnValue;

    if (stream == NULL)
    {
        return(EOF);
    }

    if (stream->bitCount == 0)
    {
        /* buffer is empty, read another character */
        if ((returnValue = fgetc(stream->fp)) == EOF)
        {
            return EOF;
        }
        else
        {
            stream->bitCount = 8;
            stream->bitBuffer = returnValue;
        }
    }

    /* bit to return is msb in buffer */
    stream->bitCount--;
    returnValue = (stream->bitBuffer) >> (stream->bitCount);

    return (returnValue & 0x01);
}

/***************************************************************************
*   Function   : BitFilePutBit
*   Description: This function writes the bit passed as a parameter to the
*                file passed a parameter.
*   Parameters : c - the bit value to be written
*                stream - pointer to bit file stream to write to
*   Effects    : Writes a bit to the bit buffer.  If the buffer has a byte,
*                the buffer is written to the file and cleared.
*   Returned   : On success, the bit value written, otherwise EOF.
***************************************************************************/
int BitFilePutBit(const int c, bit_file_t *stream)
{
    int returnValue = c;

    if (stream == NULL)
    {
        return(EOF);
    }

    stream->bitCount++;
    stream->bitBuffer <<= 1;

    if (c != 0)
    {
        stream->bitBuffer |= 1;
    }

    /* write bit buffer if we have 8 bits */
    if (stream->bitCount == 8)
    {
        if (fputc(stream->bitBuffer, stream->fp) == EOF)
        {
            returnValue = EOF;
        }

        /* reset buffer */
        stream->bitCount = 0;
        stream->bitBuffer = 0;
    }

    return returnValue;
}

/***************************************************************************
*   Function   : BitFileGetBits
*   Description: This function reads the specified number of bits from the
*                file passed as a parameter and writes them to the
*                requested memory location (msb to lsb).
*   Parameters : stream - pointer to bit file stream to read from
*                bits - address to store bits read
*                count - number of bits to read
*   Effects    : Reads bits from the bit buffer and file stream.  The bit
*                buffer will be modified as necessary.
*   Returned   : EOF for failure, otherwise the number of bits read.  If
*                an EOF is reached before all the bits are read, bits
*                will contain every bit through the last complete byte.
***************************************************************************/
int BitFileGetBits(bit_file_t *stream, void *bits, const unsigned int count)
{
    unsigned char *bytes, shifts;
    int offset, remaining, returnValue;

    bytes = (unsigned char *)bits;

    if ((stream == NULL) || (bits == NULL))
    {
        return(EOF);
    }

    offset = 0;
    remaining = count;

    /* read whole bytes */
    while (remaining >= 8)
    {
        returnValue = BitFileGetChar(stream);

        if (returnValue == EOF)
        {
            return EOF;
        }

        bytes[offset] = (unsigned char)returnValue;
        remaining -= 8;
        offset++;
    }

    if (remaining != 0)
    {
        /* read remaining bits */
        shifts = 8 - remaining;
        bytes[offset] = 0;

        while (remaining > 0)
        {
            returnValue = BitFileGetBit(stream);

            if (returnValue == EOF)
            {
                return EOF;
            }

            bytes[offset] <<= 1;
            bytes[offset] |= (returnValue & 0x01);
            remaining--;
        }

        /* shift last bits into position */
        bytes[offset] <<= shifts;
    }

    return count;
}

/***************************************************************************
*   Function   : BitFilePutBits
*   Description: This function writes the specified number of bits from the
*                memory location passed as a parameter to the file passed
*                as a parameter.   Bits are written msb to lsb.
*   Parameters : stream - pointer to bit file stream to write to
*                bits - pointer to bits to write
*                count - number of bits to write
*   Effects    : Writes bits to the bit buffer and file stream.  The bit
*                buffer will be modified as necessary.
*   Returned   : EOF for failure, otherwise the number of bits written.  If
*                an error occurs after a partial write, the partially
*                written bits will not be unwritten.
***************************************************************************/
int BitFilePutBits(bit_file_t *stream, void *bits, const unsigned int count)
{
    unsigned char *bytes, tmp;
    int offset, remaining, returnValue;

    bytes = (unsigned char *)bits;

    if ((stream == NULL) || (bits == NULL))
    {
        return(EOF);
    }

    offset = 0;
    remaining = count;

    /* write whole bytes */
    while (remaining >= 8)
    {
        returnValue = BitFilePutChar(bytes[offset], stream);

        if (returnValue == EOF)
        {
            return EOF;
        }

        remaining -= 8;
        offset++;
    }

    if (remaining != 0)
    {
        /* write remaining bits */
        tmp = bytes[offset];

        while (remaining > 0)
        {
            returnValue = BitFilePutBit((tmp & 0x80), stream);

            if (returnValue == EOF)
            {
                return EOF;
            }

            tmp <<= 1;
            remaining--;
        }
    }

    return count;
}

/***************************************************************************
*   Function   : BitFileGetBitsNum
*   Description: This function provides a machine independent layer that
*                allows a single function call to stuff an arbitrary number
*                of bits into an integer type variable.
*   Parameters : stream - pointer to bit file stream to read from
*                bits - address to store bits read
*                count - number of bits to read
*                size - sizeof type containing "bits"
*   Effects    : Calls a function that reads bits from the bit buffer and
*                file stream.  The bit buffer will be modified as necessary.
*                the bits will be written to "bits" from least significant
*                byte to most significant byte.
*   Returned   : EOF for failure, -ENOTSUP unsupported architecture,
*                otherwise the number of bits read by the called function.
***************************************************************************/
int BitFileGetBitsNum(bit_file_t *stream, void *bits, const unsigned int count,
    const size_t size)
{
    if ((stream == NULL) || (bits == NULL))
    {
        return EOF;
    }

    if (NULL == stream->GetBitsNumFunc)
    {
        return -ENOTSUP;
    }

    /* call function that correctly handles endianess */
    return (stream->GetBitsNumFunc)(stream, bits, count, size);
}

/***************************************************************************
*   Function   : BitFileGetBitsLE   (Little Endian)
*   Description: This function reads the specified number of bits from the
*                file passed as a parameter and writes them to the
*                requested memory location (LSB to MSB).
*   Parameters : stream - pointer to bit file stream to read from
*                bits - address to store bits read
*                count - number of bits to read
*                size - sizeof type containing "bits"
*   Effects    : Reads bits from the bit buffer and file stream.  The bit
*                buffer will be modified as necessary.  bits is treated as
*                a little endian integer of length >= (count/8) + 1.
*   Returned   : EOF for failure, otherwise the number of bits read.  If
*                an EOF is reached before all the bits are read, bits
*                will contain every bit through the last successful read.
***************************************************************************/
static int BitFileGetBitsLE(bit_file_t *stream, void *bits,
    const unsigned int count, const size_t size)
{
    unsigned char *bytes;
    int offset, remaining, returnValue;

    (void)size;
    bytes = (unsigned char *)bits;
    offset = 0;
    remaining = count;

    /* read whole bytes */
    while (remaining >= 8)
    {
        returnValue = BitFileGetChar(stream);

        if (returnValue == EOF)
        {
            return EOF;
        }

        bytes[offset] = (unsigned char)returnValue;
        remaining -= 8;
        offset++;
    }

    if (remaining != 0)
    {
        /* read remaining bits */
        while (remaining > 0)
        {
            returnValue = BitFileGetBit(stream);

            if (returnValue == EOF)
            {
                return EOF;
            }

            bytes[offset] <<= 1;
            bytes[offset] |= (returnValue & 0x01);
            remaining--;
        }

    }

    return count;
}

/***************************************************************************
*   Function   : BitFileGetBitsBE   (Big Endian)
*   Description: This function reads the specified number of bits from the
*                file passed as a parameter and writes them to the
*                requested memory location (LSB to MSB).
*   Parameters : stream - pointer to bit file stream to read from
*                bits - address to store bits read
*                count - number of bits to read
*                size - sizeof type containing "bits"
*   Effects    : Reads bits from the bit buffer and file stream.  The bit
*                buffer will be modified as necessary.  bits is treated as
*                a big endian integer of length size.
*   Returned   : EOF for failure, otherwise the number of bits read.  If
*                an EOF is reached before all the bits are read, bits
*                will contain every bit through the last successful read.
***************************************************************************/
static int BitFileGetBitsBE(bit_file_t *stream, void *bits,
    const unsigned int count, const size_t size)
{
    unsigned char *bytes;
    int offset, remaining, returnValue;

    if (count > (size * 8))
    {
        /* too many bits to read */
        return EOF;
    }

    bytes = (unsigned char *)bits;

    offset = size - 1;
    remaining = count;

    /* read whole bytes */
    while (remaining >= 8)
    {
        returnValue = BitFileGetChar(stream);

        if (returnValue == EOF)
        {
            return EOF;
        }

        bytes[offset] = (unsigned char)returnValue;
        remaining -= 8;
        offset--;
    }

    if (remaining != 0)
    {
        /* read remaining bits */
        while (remaining > 0)
        {
            returnValue = BitFileGetBit(stream);

            if (returnValue == EOF)
            {
                return EOF;
            }

            bytes[offset] <<= 1;
            bytes[offset] |= (returnValue & 0x01);
            remaining--;
        }

    }

    return count;
}

/***************************************************************************
*   Function   : BitFilePutBitsNum
*   Description: This function provides a machine independent layer that
*                allows a single function call to write an arbitrary number
*                of bits from an integer type variable into a file.
*   Parameters : stream - pointer to bit file stream to write to
*                bits - pointer to bits to write
*                count - number of bits to write
*                size - sizeof type containing "bits"
*   Effects    : Calls a function that writes bits to the bit buffer and
*                file stream.  The bit buffer will be modified as necessary.
*                the bits will be written to the file stream from least
*                significant byte to most significant byte.
*   Returned   : EOF for failure, ENOTSUP unsupported architecture,
*                otherwise the number of bits written.  If an error occurs
*                after a partial write, the partially written bits will not
*                be unwritten.
***************************************************************************/
int BitFilePutBitsNum(bit_file_t *stream, void *bits, const unsigned int count,
    const size_t size)
{
    if ((stream == NULL) || (bits == NULL))
    {
        return EOF;
    }

    if (NULL == stream->PutBitsNumFunc)
    {
        return ENOTSUP;
    }

    /* call function that correctly handles endianess */
    return (stream->PutBitsNumFunc)(stream, bits, count, size);
}

/***************************************************************************
*   Function   : BitFilePutBitsLE   (Little Endian)
*   Description: This function writes the specified number of bits from the
*                memory location passed as a parameter to the file passed
*                as a parameter.   Bits are written LSB to MSB.
*   Parameters : stream - pointer to bit file stream to write to
*                bits - pointer to bits to write
*                count - number of bits to write
*                size - sizeof type containing "bits"
*   Effects    : Writes bits to the bit buffer and file stream.  The bit
*                buffer will be modified as necessary.  bits is treated as
*                a little endian integer of length >= (count/8) + 1.
*   Returned   : EOF for failure, otherwise the number of bits written.  If
*                an error occurs after a partial write, the partially
*                written bits will not be unwritten.
***************************************************************************/
static int BitFilePutBitsLE(bit_file_t *stream, void *bits,
    const unsigned int count, const size_t size)
{
    unsigned char *bytes, tmp;
    int offset, remaining, returnValue;

    (void)size;
    bytes = (unsigned char *)bits;
    offset = 0;
    remaining = count;

    /* write whole bytes */
    while (remaining >= 8)
    {
        returnValue = BitFilePutChar(bytes[offset], stream);

        if (returnValue == EOF)
        {
            return EOF;
        }

        remaining -= 8;
        offset++;
    }

    if (remaining != 0)
    {
        /* write remaining bits */
        tmp = bytes[offset];
        tmp <<= (8 - remaining);

        while (remaining > 0)
        {
            returnValue = BitFilePutBit((tmp & 0x80), stream);

            if (returnValue == EOF)
            {
                return EOF;
            }

            tmp <<= 1;
            remaining--;
        }
    }

    return count;
}

/***************************************************************************
*   Function   : BitFilePutBitsBE   (Big Endian)
*   Description: This function writes the specified number of bits from the
*                memory location passed as a parameter to the file passed
*                as a parameter.   Bits are written LSB to MSB.
*   Parameters : stream - pointer to bit file stream to write to
*                bits - pointer to bits to write
*                count - number of bits to write
*   Effects    : Writes bits to the bit buffer and file stream.  The bit
*                buffer will be modified as necessary.  bits is treated as
*                a big endian integer of length size.
*   Returned   : EOF for failure, otherwise the number of bits written.  If
*                an error occurs after a partial write, the partially
*                written bits will not be unwritten.
***************************************************************************/
static int BitFilePutBitsBE(bit_file_t *stream, void *bits,
    const unsigned int count, const size_t size)
{
    unsigned char *bytes, tmp;
    int offset, remaining, returnValue;

    if (count > (size * 8))
    {
        /* too many bits to write */
        return EOF;
    }

    bytes = (unsigned char *)bits;
    offset = size - 1;
    remaining = count;

    /* write whole bytes */
    while (remaining >= 8)
    {
        returnValue = BitFilePutChar(bytes[offset], stream);

        if (returnValue == EOF)
        {
            return EOF;
        }

        remaining -= 8;
        offset--;
    }

    if (remaining != 0)
    {
        /* write remaining bits */
        tmp = bytes[offset];
        tmp <<= (8 - remaining);

        while (remaining > 0)
        {
            returnValue = BitFilePutBit((tmp & 0x80), stream);

            if (returnValue == EOF)
            {
                return EOF;
            }

            tmp <<= 1;
            remaining--;
        }
    }

    return count;
}

/***************************************************************************
*   Function   : BitFileNotSupported
*   Description: This function returns -ENOTSUP.  It is called when a
*                Get/PutBits function is called on an unsupported
*                architecture.
*   Parameters : stream - not used
*                bits - not used
*                count - not used
*   Effects    : None
*   Returned   : -ENOTSUP
***************************************************************************/
static int BitFileNotSupported(bit_file_t *stream, void *bits,
    const unsigned int count, const size_t size)
{
    (void)stream;
    (void)bits;
    (void)count;
    (void)size;

    return -ENOTSUP;
}

/***************************************************************************
*                             OPTLIST.C
***************************************************************************/

/***************************************************************************
*                             INCLUDED FILES
***************************************************************************/
#include "optlist.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/***************************************************************************
*                            TYPE DEFINITIONS
***************************************************************************/

/***************************************************************************
*                                CONSTANTS
***************************************************************************/

/***************************************************************************
*                            GLOBAL VARIABLES
***************************************************************************/

/***************************************************************************
*                               PROTOTYPES
***************************************************************************/
static option_t *MakeOpt(
    const char option, char *const argument, const int index);

static size_t MatchOpt(
    const char argument, char *const options);

/***************************************************************************
*                                FUNCTIONS
***************************************************************************/

/****************************************************************************
*   Function   : GetOptList
*   Description: This function is similar to the POSIX function getopt.  All
*                options and their corresponding arguments are returned in a
*                linked list.  This function should only be called once per
*                an option list and it does not modify argv or argc.
*   Parameters : argc - the number of command line arguments (including the
*                       name of the executable)
*                argv - pointer to the open binary file to write encoded
*                       output
*                options - getopt style option list.  A NULL terminated
*                          string of single character options.  Follow an
*                          option with a colon to indicate that it requires
*                          an argument.
*   Effects    : Creates a link list of command line options and their
*                arguments.
*   Returned   : option_t type value where the option and arguement fields
*                contain the next option symbol and its argument (if any).
*                The argument field will be set to NULL if the option is
*                specified as having no arguments or no arguments are found.
*                The option field will be set to PO_NO_OPT if no more
*                options are found.
*
*   NOTE: The caller is responsible for freeing up the option list when it
*         is no longer needed.
****************************************************************************/
option_t *GetOptList(const int argc, char *const argv[], char *const options)
{
    int nextArg;
    option_t *head, *tail;
    size_t optIndex;
    size_t argIndex;

    /* start with first argument and nothing found */
    nextArg = 1;
    head = NULL;
    tail = NULL;

    /* loop through all of the command line arguments */
    while (nextArg < argc)
    {
        argIndex = 1;

        while ((strlen(argv[nextArg]) > argIndex) && ('-' == argv[nextArg][0]))
        {
            /* attempt to find a matching option */
            optIndex = MatchOpt(argv[nextArg][argIndex], options);

            if (options[optIndex] == argv[nextArg][argIndex])
            {
                /* we found the matching option */
                if (NULL == head)
                {
                    head = MakeOpt(options[optIndex], NULL, OL_NOINDEX);
                    tail = head;
                }
                else
                {
                    tail->next = MakeOpt(options[optIndex], NULL, OL_NOINDEX);
                    tail = tail->next;
                }

                if (':' == options[optIndex + 1])
                {
                    /* the option found should have a text arguement */
                    argIndex++;

                    if (strlen(argv[nextArg]) > argIndex)
                    {
                        /* no space between argument and option */
                        tail->argument = &(argv[nextArg][argIndex]);
                        tail->argIndex = nextArg;
                    }
                    else if (nextArg < argc)
                    {
                        /* there must be space between the argument option */
                        nextArg++;
                        tail->argument = argv[nextArg];
                        tail->argIndex = nextArg;
                    }

                    break; /* done with argv[nextArg] */
                }
            }

            argIndex++;
        }

        nextArg++;
    }

    return head;
}

/****************************************************************************
*   Function   : MakeOpt
*   Description: This function uses malloc to allocate space for an option_t
*                type structure and initailizes the structure with the
*                values passed as a parameter.
*   Parameters : option - this option character
*                argument - pointer string containg the argument for option.
*                           Use NULL for no argument
*                index - argv[index] contains argument use OL_NOINDEX for
*                        no argument
*   Effects    : A new option_t type variable is created on the heap.
*   Returned   : Pointer to newly created and initialized option_t type
*                structure.  NULL if space for structure can't be allocated.
****************************************************************************/
static option_t *MakeOpt(
    const char option, char *const argument, const int index)
{
    option_t *opt;

    opt = (option_t *)malloc(sizeof(option_t));

    if (opt != NULL)
    {
        opt->option = option;
        opt->argument = argument;
        opt->argIndex = index;
        opt->next = NULL;
    }
    else
    {
        perror("Failed to Allocate option_t");
    }

    return opt;
}

/****************************************************************************
*   Function   : FreeOptList
*   Description: This function will free all the elements in an option_t
*                type linked list starting from the node passed as a
*                parameter.
*   Parameters : list - head of linked list to be freed
*   Effects    : All elements of the linked list pointed to by list will
*                be freed and list will be set to NULL.
*   Returned   : None
****************************************************************************/
void FreeOptList(option_t *list)
{
    option_t *head, *next;

    head = list;
    list = NULL;

    while (head != NULL)
    {
        next = head->next;
        free(head);
        head = next;
    }

    return;
}

/****************************************************************************
*   Function   : MatchOpt
*   Description: This function searches for an arguement in an option list.
*                It will return the index to the option matching the
*                arguement or the index to the NULL if none is found.
*   Parameters : arguement - character arguement to be matched to an
*                            option in the option list
*                options - getopt style option list.  A NULL terminated
*                          string of single character options.  Follow an
*                          option with a colon to indicate that it requires
*                          an argument.
*   Effects    : None
*   Returned   : Index of argument in option list.  Index of end of string
*                if arguement does not appear in the option list.
****************************************************************************/
static size_t MatchOpt(
    const char argument, char *const options)
{
    size_t optIndex = 0;

    /* attempt to find a matching option */
    while ((options[optIndex] != '\0') &&
        (options[optIndex] != argument))
    {
        do
        {
            optIndex++;
        }
        while ((options[optIndex] != '\0') &&
            (':' == options[optIndex]));
    }

    return optIndex;
}

/****************************************************************************
*   Function   : FindFileName
*   Description: This is function accepts a pointer to the name of a file
*                along with path information and returns a pointer to the
*                first character that is not part of the path.
*   Parameters : fullPath - pointer to an array of characters containing
*                           a file name and possible path modifiers.
*   Effects    : None
*   Returned   : Returns a pointer to the first character after any path
*                information.
****************************************************************************/
char *FindFileName(const char *const fullPath)
{
    int i;
    const char *start;                          /* start of file name */
    const char *tmp;
    const char delim[3] = {'\\', '/', ':'};     /* path deliminators */

    start = fullPath;

    /* find the first character after all file path delimiters */
    for (i = 0; i < 3; i++)
    {
        tmp = strrchr(start, delim[i]);

        if (tmp != NULL)
        {
            start = tmp + 1;
        }
    }

    return (char *)start;
}

/***************************************************************************
*                                 BRUTE.C
***************************************************************************/

/***************************************************************************
*                             INCLUDED FILES
***************************************************************************/
#include "lzlocal.h"
#include <PFAC.h>
#include <assert.h>

/***************************************************************************
*                            GLOBAL VARIABLES
***************************************************************************/
/* cyclic buffer sliding window of already read characters */
extern unsigned char slidingWindow[];
extern unsigned char uncodedLookahead[];

/***************************************************************************
*                                FUNCTIONS
***************************************************************************/

/****************************************************************************
*   Function   : InitializeSearchStructures
*   Description: This function initializes structures used to speed up the
*                process of mathcing uncoded strings to strings in the
*                sliding window.  The brute force search doesn't use any
*                special structures, so this function doesn't do anything.
*   Parameters : None
*   Effects    : None
*   Returned   : 0 for success, -1 for failure.  errno will be set in the
*                event of a failure.
****************************************************************************/
int InitializeSearchStructures(void)
{
    return 0;
}

/****************************************************************************
*   Function   : FindMatch
*   Description: This function will search through the slidingWindow
*                dictionary for the longest sequence matching the MAX_CODED
*                long string stored in uncodedLookahed.
*   Parameters : windowHead - head of sliding window
*                uncodedHead - head of uncoded lookahead buffer
*   Effects    : None
*   Returned   : The sliding window index where the match starts and the
*                length of the match.  If there is no match a length of
*                zero will be returned.
****************************************************************************/
encoded_string_t FindMatch(const unsigned int windowHead,
    unsigned int uncodedHead)
{
    encoded_string_t matchData;
    unsigned int i;
    unsigned int j;

    matchData.length = 0;
    matchData.offset = 0;
    i = windowHead;  /* start at the beginning of the sliding window */
    //j = 0;

    PFAC_handle_t handle ;
    PFAC_status_t PFAC_status ;
    int input_size = strlen((char *)slidingWindow);    
    char *h_inputString = NULL ;
    int  *h_matched_result = NULL ;

    // step 1: create PFAC handle 
    PFAC_status = PFAC_create( &handle ) ;
	//handle->textureMode = PFAC_TEXTURE_OFF; 
    assert( PFAC_STATUS_SUCCESS == PFAC_status );
    
    // step 2: read patterns and dump transition table 
    PFAC_status = PFAC_readPattern( handle, (char *)uncodedLookahead) ;
    if ( PFAC_STATUS_SUCCESS != PFAC_status ){
        printf("Error: fails to read pattern from file, %s\n", PFAC_getErrorString(PFAC_status) );
        exit(1) ;	
    }
    
    // allocate memory to contain the whole file
    h_inputString = (char *) malloc (sizeof(char)*input_size);
    assert( NULL != h_inputString );
 
    h_matched_result = (int *) malloc (sizeof(int)*input_size);
    assert( NULL != h_matched_result );
    memset( h_matched_result, 0, sizeof(int)*input_size ) ;
     
    // copy the file into the buffer
    strcpy(h_inputString, (char *)slidingWindow);
    
    // step 4: run PFAC on GPU           
    PFAC_status = PFAC_matchFromHost( handle, h_inputString, input_size, h_matched_result ) ;
    if ( PFAC_STATUS_SUCCESS != PFAC_status ){
        printf("Error: fails to PFAC_matchFromHost, %s\n", PFAC_getErrorString(PFAC_status) );
        exit(1) ;	
    }     

    // step 5: output matched result
    for (i = 0; i < input_size; i++) {
        if (h_matched_result[i] != 0) {
            if(h_matched_result[i] > matchData.length){
        		matchData.length = h_matched_result[i];
    			matchData.offset = windowHead-h_matched_result[i];
            }
        }
    }

    PFAC_status = PFAC_destroy( handle ) ;
    assert( PFAC_STATUS_SUCCESS == PFAC_status );
    
    free(h_inputString);
    free(h_matched_result); 

    return matchData;
}

/****************************************************************************
*   Function   : ReplaceChar
*   Description: This function replaces the character stored in
*                slidingWindow[charIndex] with the one specified by
*                replacement.
*   Parameters : charIndex - sliding window index of the character to be
*                            removed from the linked list.
*   Effects    : slidingWindow[charIndex] is replaced by replacement.
*   Returned   : 0 for success, -1 for failure.  errno will be set in the
*                event of a failure.
****************************************************************************/
int ReplaceChar(const unsigned int charIndex, const unsigned char replacement)
{
    slidingWindow[charIndex] = replacement;
    return 0;
}

/***************************************************************************
*                                 LZSS.C
***************************************************************************/

/***************************************************************************
*                             INCLUDED FILES
***************************************************************************/
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "lzlocal.h"
#include "bitfile.h"

/***************************************************************************
*                            TYPE DEFINITIONS
***************************************************************************/

/***************************************************************************
*                                CONSTANTS
***************************************************************************/

/***************************************************************************
*                            GLOBAL VARIABLES
***************************************************************************/
/* cyclic buffer sliding window of already read characters */
unsigned char slidingWindow[WINDOW_SIZE];
unsigned char uncodedLookahead[MAX_CODED];

/***************************************************************************
*                               PROTOTYPES
***************************************************************************/

/***************************************************************************
*                                FUNCTIONS
***************************************************************************/

/****************************************************************************
*   Function   : EncodeLZSS
*   Description: This function will read an input file and write an output
*                file encoded according to the traditional LZSS algorithm.
*                This algorithm encodes strings as 16 bits (a 12 bit offset
*                + a 4 bit length).
*   Parameters : fpIn - pointer to the open binary file to encode
*                fpOut - pointer to the open binary file to write encoded
*                       output
*   Effects    : fpIn is encoded and written to fpOut.  Neither file is
*                closed after exit.
*   Returned   : 0 for success, -1 for failure.  errno will be set in the
*                event of a failure.
****************************************************************************/
int EncodeLZSS(FILE *fpIn, FILE *fpOut)
{
    bit_file_t *bfpOut;
    encoded_string_t matchData;
    int c;
    unsigned int i;
    unsigned int len;                       /* length of string */

    /* head of sliding window and lookahead */
    unsigned int windowHead, uncodedHead;

    /* validate arguments */
    if ((NULL == fpIn) || (NULL == fpOut))
    {
        errno = ENOENT;
        return -1;
    }

    /* convert output file to bitfile */
    bfpOut = MakeBitFile(fpOut, BF_WRITE);

    if (NULL == bfpOut)
    {
        perror("Making Output File a BitFile");
        return -1;
    }

    windowHead = 0;
    uncodedHead = 0;

    /************************************************************************
    * Fill the sliding window buffer with some known vales.  DecodeLZSS must
    * use the same values.  If common characters are used, there's an
    * increased chance of matching to the earlier strings.
    ************************************************************************/
    memset(slidingWindow, ' ', WINDOW_SIZE * sizeof(unsigned char));

    /************************************************************************
    * Copy MAX_CODED bytes from the input file into the uncoded lookahead
    * buffer.
    ************************************************************************/
    for (len = 0; len < MAX_CODED && (c = getc(fpIn)) != EOF; len++)
    {
        uncodedLookahead[len] = c;
    }

    if (0 == len)
    {
        return 0;   /* inFile was empty */
    }

    /* Look for matching string in sliding window */
    i = InitializeSearchStructures();

    if (0 != i)
    {
        return i;       /* InitializeSearchStructures returned an error */
    }

    matchData = FindMatch(windowHead, uncodedHead);

    /* now encoded the rest of the file until an EOF is read */
    while (len > 0)
    {
        if (matchData.length > len)
        {
            /* garbage beyond last data happened to extend match length */
            matchData.length = len;
        }

        if (matchData.length <= MAX_UNCODED)
        {
            /* not long enough match.  write uncoded flag and character */
            BitFilePutBit(UNCODED, bfpOut);
            BitFilePutChar(uncodedLookahead[uncodedHead], bfpOut);

            matchData.length = 1;   /* set to 1 for 1 byte uncoded */
        }
        else
        {
            unsigned int adjustedLen;

            /* adjust the length of the match so minimun encoded len is 0*/
            adjustedLen = matchData.length - (MAX_UNCODED + 1);

            /* match length > MAX_UNCODED.  Encode as offset and length. */
            BitFilePutBit(ENCODED, bfpOut);
            BitFilePutBitsNum(bfpOut, &matchData.offset, OFFSET_BITS,
                sizeof(unsigned int));
            BitFilePutBitsNum(bfpOut, &adjustedLen, LENGTH_BITS,
                sizeof(unsigned int));
        }

        /********************************************************************
        * Replace the matchData.length worth of bytes we've matched in the
        * sliding window with new bytes from the input file.
        ********************************************************************/
        i = 0;
        while ((i < matchData.length) && ((c = getc(fpIn)) != EOF))
        {
            /* add old byte into sliding window and new into lookahead */
            ReplaceChar(windowHead, uncodedLookahead[uncodedHead]);
            uncodedLookahead[uncodedHead] = c;
            windowHead = Wrap((windowHead + 1), WINDOW_SIZE);
            uncodedHead = Wrap((uncodedHead + 1), MAX_CODED);
            i++;
        }

        /* handle case where we hit EOF before filling lookahead */
        while (i < matchData.length)
        {
            ReplaceChar(windowHead, uncodedLookahead[uncodedHead]);
            /* nothing to add to lookahead here */
            windowHead = Wrap((windowHead + 1), WINDOW_SIZE);
            uncodedHead = Wrap((uncodedHead + 1), MAX_CODED);
            len--;
            i++;
        }

        /* find match for the remaining characters */
        matchData = FindMatch(windowHead, uncodedHead);
    }

    /* we've encoded everything, free bitfile structure */
    BitFileToFILE(bfpOut);

   return 0;
}

/****************************************************************************
*   Function   : DecodeLZSSByFile
*   Description: This function will read an LZSS encoded input file and
*                write an output file.  This algorithm encodes strings as 16
*                bits (a 12 bit offset + a 4 bit length).
*   Parameters : fpIn - pointer to the open binary file to decode
*                fpOut - pointer to the open binary file to write decoded
*                       output
*   Effects    : fpIn is decoded and written to fpOut.  Neither file is
*                closed after exit.
*   Returned   : 0 for success, -1 for failure.  errno will be set in the
*                event of a failure.
****************************************************************************/
int DecodeLZSS(FILE *fpIn, FILE *fpOut)
{
    bit_file_t *bfpIn;
    int c;
    unsigned int i, nextChar;
    encoded_string_t code;              /* offset/length code for string */

    /* use stdin if no input file */
    if ((NULL == fpIn) || (NULL == fpOut))
    {
        errno = ENOENT;
        return -1;
    }

    /* convert input file to bitfile */
    bfpIn = MakeBitFile(fpIn, BF_READ);

    if (NULL == bfpIn)
    {
        perror("Making Input File a BitFile");
        return -1;
    }

    /************************************************************************
    * Fill the sliding window buffer with some known vales.  EncodeLZSS must
    * use the same values.  If common characters are used, there's an
    * increased chance of matching to the earlier strings.
    ************************************************************************/
    memset(slidingWindow, ' ', WINDOW_SIZE * sizeof(unsigned char));

    nextChar = 0;

    while (1)
    {
        if ((c = BitFileGetBit(bfpIn)) == EOF)
        {
            /* we hit the EOF */
            break;
        }

        if (c == UNCODED)
        {
            /* uncoded character */
            if ((c = BitFileGetChar(bfpIn)) == EOF)
            {
                break;
            }

            /* write out byte and put it in sliding window */
            putc(c, fpOut);
            slidingWindow[nextChar] = c;
            nextChar = Wrap((nextChar + 1), WINDOW_SIZE);
        }
        else
        {
            /* offset and length */
            code.offset = 0;
            code.length = 0;

            if ((BitFileGetBitsNum(bfpIn, &code.offset, OFFSET_BITS,
                sizeof(unsigned int))) == EOF)
            {
                break;
            }

            if ((BitFileGetBitsNum(bfpIn, &code.length, LENGTH_BITS,
                sizeof(unsigned int))) == EOF)
            {
                break;
            }

            code.length += MAX_UNCODED + 1;

            /****************************************************************
            * Write out decoded string to file and lookahead.  It would be
            * nice to write to the sliding window instead of the lookahead,
            * but we could end up overwriting the matching string with the
            * new string if abs(offset - next char) < match length.
            ****************************************************************/
            for (i = 0; i < code.length; i++)
            {
                c = slidingWindow[Wrap((code.offset + i), WINDOW_SIZE)];
                putc(c, fpOut);
                uncodedLookahead[i] = c;
            }

            /* write out decoded string to sliding window */
            for (i = 0; i < code.length; i++)
            {
                slidingWindow[Wrap((nextChar + i), WINDOW_SIZE)] =
                    uncodedLookahead[i];
            }

            nextChar = Wrap((nextChar + code.length), WINDOW_SIZE);
        }
    }

    /* we've decoded everything, free bitfile structure */
    BitFileToFILE(bfpIn);

    return 0;
}

/***************************************************************************
*                                SAMPLE.C
***************************************************************************/

/***************************************************************************
*                             INCLUDED FILES
***************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lzss.h"
#include "optlist.h"

/***************************************************************************
*                            TYPE DEFINITIONS
***************************************************************************/
typedef enum
{
    ENCODE,
    DECODE
} modes_t;

/***************************************************************************
*                                CONSTANTS
***************************************************************************/

/***************************************************************************
*                            GLOBAL VARIABLES
***************************************************************************/

/***************************************************************************
*                               PROTOTYPES
***************************************************************************/

/***************************************************************************
*                                FUNCTIONS
***************************************************************************/

/****************************************************************************
*   Function   : main
*   Description: This is the main function for this program, it validates
*                the command line input and, if valid, it will either
*                encode a file using the LZSS algorithm or decode a
*                file encoded with the LZSS algorithm.
*   Parameters : argc - number of parameters
*                argv - parameter list
*   Effects    : Encodes/Decodes input file
*   Returned   : 0 for success, -1 for failure.  errno will be set in the
*                event of a failure.
****************************************************************************/
int main(int argc, char *argv[])
{
    option_t *optList;
    option_t *thisOpt;
    FILE *fpIn;             /* pointer to open input file */
    FILE *fpOut;            /* pointer to open output file */
    modes_t mode;

    /* initialize data */
    fpIn = NULL;
    fpOut = NULL;
    mode = ENCODE;

    /* parse command line */
    optList = GetOptList(argc, argv, "cdi:o:h?");
    thisOpt = optList;

    while (thisOpt != NULL)
    {
        switch(thisOpt->option)
        {
            case 'c':       /* compression mode */
                mode = ENCODE;
                break;

            case 'd':       /* decompression mode */
                mode = DECODE;
                break;

            case 'i':       /* input file name */
                if (fpIn != NULL)
                {
                    fprintf(stderr, "Multiple input files not allowed.\n");
                    fclose(fpIn);

                    if (fpOut != NULL)
                    {
                        fclose(fpOut);
                    }

                    FreeOptList(optList);
                    return -1;
                }

                /* open input file as binary */
                fpIn = fopen(thisOpt->argument, "rb");
                if (fpIn == NULL)
                {
                    perror("Opening input file");

                    if (fpOut != NULL)
                    {
                        fclose(fpOut);
                    }

                    FreeOptList(optList);
                    return -1;
                }
                break;

            case 'o':       /* output file name */
                if (fpOut != NULL)
                {
                    fprintf(stderr, "Multiple output files not allowed.\n");
                    fclose(fpOut);

                    if (fpIn != NULL)
                    {
                        fclose(fpIn);
                    }

                    FreeOptList(optList);
                    return -1;
                }

                /* open output file as binary */
                fpOut = fopen(thisOpt->argument, "wb");
                if (fpOut == NULL)
                {
                    perror("Opening output file");

                    if (fpIn != NULL)
                    {
                        fclose(fpIn);
                    }

                    FreeOptList(optList);
                    return -1;
                }
                break;

            case 'h':
            case '?':
                printf("Usage: %s <options>\n\n", FindFileName(argv[0]));
                printf("options:\n");
                printf("  -c : Encode input file to output file.\n");
                printf("  -d : Decode input file to output file.\n");
                printf("  -i <filename> : Name of input file.\n");
                printf("  -o <filename> : Name of output file.\n");
                printf("  -h | ?  : Print out command line options.\n\n");
                printf("Default: %s -c -i stdin -o stdout\n",
                    FindFileName(argv[0]));

                FreeOptList(optList);
                return 0;
        }

        optList = thisOpt->next;
        free(thisOpt);
        thisOpt = optList;
    }

    /* use stdin/out if no files are provided */
    if (fpIn == NULL)
    {
        fpIn = stdin;
    }

    if (fpOut == NULL)
    {
        fpOut = stdout;
    }

    /* we have valid parameters encode or decode */
    if (mode == ENCODE)
    {
        EncodeLZSS(fpIn, fpOut);
    }
    else
    {
        DecodeLZSS(fpIn, fpOut);
    }

    /* remember to close files */
    fclose(fpIn);
    fclose(fpOut);
    return 0;
}