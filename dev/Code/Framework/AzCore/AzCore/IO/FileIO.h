/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/
#pragma once
#ifndef AZCORE_IO_FILEIO_H
#define AZCORE_IO_FILEIO_H

#include <AzCore/base.h>
#include <AzCore/std/function/function_fwd.h>
#include <AzCore/IO/GenericStreams.h>
#include <AzCore/std/string/string.h>
#include <AzCore/Memory/SystemAllocator.h>

// this header contains the interface FileIOBase, a base class to derive
// from in order to implement the low level file operations that the engine uses
// all file access by the engine should through a FileIOBase interface-derived class
// which is provided in the global environment at "EngineFileIO"
// To access it use the GetInstance/SetInstance functions in FileIOBase.

namespace AZ
{
    namespace IO
    {
        // Utility functions

        /// return true if name of file matches glob filter.
        /// glob filters are MS-DOS (or windows) findNextFile style filters
        /// like "*.bat" or "blah??.pak" or "test*.exe" and such.
        bool NameMatchesFilter(const char* name, const char* filter);

        typedef AZ::u32 HandleType; // typedef to be compatible with VS2012
        static const HandleType InvalidHandle = 0;
        static const size_t MaxPathLength = 1024U;

        enum class SeekType : AZ::u32
        {
            SeekFromStart,
            SeekFromCurrent,
            SeekFromEnd
        };

        SeekType GetSeekTypeFromFSeekMode(int mode);
        int GetFSeekModeFromSeekType(SeekType type);

        enum class OpenMode : AZ::u32
        {
            Invalid = 0,
            ModeRead = (1 << 0),
            ModeWrite = (1 << 1),
            ModeAppend = (1 << 2),
            ModeBinary = (1 << 3),
            ModeText = (1 << 4),
            ModeUpdate = (1 << 5),
            ModeCreatePath = (1 << 6),
        };

        inline bool AnyFlag(OpenMode a)
        {
            return a != OpenMode::Invalid;
        }

        inline OpenMode operator | (OpenMode a, OpenMode b)
        {
            return static_cast<OpenMode>(static_cast<AZ::u32>(a) | static_cast<AZ::u32>(b));
        }

        inline OpenMode operator & (OpenMode a, OpenMode b)
        {
            return static_cast<OpenMode>(static_cast<AZ::u32>(a) & static_cast<AZ::u32>(b));
        }

        inline OpenMode& operator |= (OpenMode& a, OpenMode b)
        {
            return a = a | b;
        }

        inline OpenMode& operator &= (OpenMode& a, OpenMode b)
        {
            return a = a & b;
        }

        OpenMode GetOpenModeFromStringMode(const char* mode);

        const char* GetStringModeFromOpenMode(OpenMode mode);

        // when reading, we ignore text mode requests, becuase text mode will not be supported from reading from
        // odd devices such as pack files, remote reads, and within compressed volumes anyway.  This makes it behave the same as
        // pak mode as it behaves loose.  In practice, developers should avoid depending on text mode for reading, specifically depending
        // on the platform-specific concept of removing '\r' characters from a text stream.
        // having the OS swallow characters without letting us know also messes up things like lookahead cache, optizing functions like FGetS and so on.
        void UpdateOpenModeForReading(OpenMode& mode);

        enum class ResultCode : AZ::u32
        {
            Success = 0,
            Error = 1,
            Error_HandleInvalid = 2
                // More results (errors) here
        };

        // a function which returns a result code and supports operator bool explicitly
        class Result
        {
        public:
            Result(ResultCode resultCode)
                : m_resultCode(resultCode)
            {
            }

            ResultCode GetResultCode() const
            {
                return *this;
            }

            operator bool() const
            {
                return m_resultCode == ResultCode::Success;
            }

            operator ResultCode() const
            {
                return m_resultCode;
            }
        private:
            ResultCode m_resultCode;
        };

        /// The base class for file IO stack classes
        class FileIOBase
        {
        public:
            virtual ~FileIOBase()
            {
            }

            /// GetInstance()
            /// A utility function to get the one global instance of File IO to use when talking to engine.
            /// returns an instance which is aware of concepts such as pak and loose files and can be used to interact with them.
            /// Note that the PAK system itself should not call GetInstance but instead should use the direct underlying IO system
            /// using GetDirectInstance instead since otherwise an infinite loop will be caused.
            static FileIOBase* GetInstance();
            static void SetInstance(FileIOBase* instance);

            /// Direct IO is the underlying IO system and does not route through pack files.  This should only be used by the
            /// pack system itself, or systems with similar needs that should be "blind" to packages and only see the real file system.
            static FileIOBase* GetDirectInstance();
            static void SetDirectInstance(FileIOBase* instance);

            // Standard operations
            virtual Result Open(const char* filePath, OpenMode mode, HandleType& fileHandle) = 0;
            virtual Result Close(HandleType fileHandle) = 0;
            virtual Result Tell(HandleType fileHandle, AZ::u64& offset) = 0;
            virtual Result Seek(HandleType fileHandle, AZ::s64 offset, SeekType type) = 0;
            virtual Result Read(HandleType fileHandle, void* buffer, AZ::u64 size, bool failOnFewerThanSizeBytesRead = false, AZ::u64* bytesRead = nullptr) = 0;
            virtual Result Write(HandleType fileHandle, const void* buffer, AZ::u64 size, AZ::u64* bytesWritten = nullptr) = 0;
            virtual Result Flush(HandleType fileHandle) = 0;
            virtual bool Eof(HandleType fileHandle) = 0;

            /// the only requirement on the ModTime functions is that it must be comparable with subsequent calls to the same function.
            /// there is no specific requirement that they be cross-platform or adhere to any standard, but they should still be comparable
            /// across sessions.
            virtual AZ::u64 ModificationTime(HandleType fileHandle) = 0;
            virtual AZ::u64 ModificationTime(const char* filePath) = 0;

            /// Get the size of the file.  Returns Success if we report size. 
            virtual Result Size(const char* filePath, AZ::u64& size) = 0;
            virtual Result Size(HandleType fileHandle, AZ::u64& size) = 0;

            /// no fail, returns false if it does not exist
            virtual bool Exists(const char* filePath) = 0;

            /// no fail, returns false if its not a directory or does not exist
            virtual bool IsDirectory(const char* filePath) = 0;

            /// no fail, returns false if its read only or does not exist
            virtual bool IsReadOnly(const char* filePath) = 0;

            /// create a path, recursively
            virtual Result CreatePath(const char* filePath) = 0;

            /// DestroyPath - Destroys the entire path and all of its contents (all files, all subdirectories, etc)
            /// succeeds if the directory was successfully destroys
            /// succeeds if the directory didn't exist in the first place
            /// fails if the path could not be destroyed (for example, if a file could not be erased for any reason)
            /// also fails if filePath refers to a file name which exists instead of a directory name.  It must be a directory name.
            virtual Result DestroyPath(const char* filePath) = 0;

            /// Remove - erases a single file
            /// succeeds if the file didn't exist to begin with
            /// succeeds if the file did exist and we successfully erased it.
            /// fails if the file cannot be erased
            /// also fails if the specified filePath is actually a directory that exists (instead of a file)
            virtual Result Remove(const char* filePath) = 0;

            /// does not overwrite the destination if it exists.
            /// note that attributes are not required to be copied
            /// the modtime of the destination path must be equal or greater than the source path
            /// but doesn't have to be equal (some operating systems forbid modifying modtimes due to security constraints)
            virtual Result Copy(const char* sourceFilePath, const char* destinationFilePath) = 0;

            /// renames a file or directory name
            /// also fails if the file or directory cannot be found
            /// also succeeds if originalFilePath == newFilePath
            virtual Result Rename(const char* originalFilePath, const char* newFilePath) = 0;

            /// FindFiles -
            /// return all files and directories matching that filter (dos-style filters) at that file path
            /// does not recurse.
            /// does not return the . or .. directories
            /// not all filters are supported.
            /// You must include <AzCore/std/functional.h> if you use this
            /// note: the callback will contain the full concatenated path (filePath + slash + fileName)
            ///       not just the individual file name found.
            /// note: if the file path of the found file corresponds to a registered ALIAS, the longest matching alias will be returned
            ///       so expect return values like @assets@/textures/mytexture.dds instead of a full path.  This is so that fileIO works over remote connections.
            /// note: if rootPath is specified the implementation has the option of substituting it for the current directory
            ///      as would be the case on a file server.
            typedef AZStd::function<bool(const char*)> FindFilesCallbackType;
            virtual Result FindFiles(const char* filePath, const char* filter, FindFilesCallbackType callback) = 0;

            // Alias system

            /// SetAlias - Adds an alias to the path resolution system, e.g. @user@, @root@, etc.
            virtual void SetAlias(const char* alias, const char* path) = 0;
            /// ClearAlias - Removes an alias from the path resolution system
            virtual void ClearAlias(const char* alias) = 0;
            /// GetAlias - Returns the destination path for a given alias, or nullptr if the alias does not exist
            virtual const char* GetAlias(const char* alias) = 0;

            /// Shorten the given path if it contains an alias.  it will always pick the longest alias match.
            /// note that it re-uses the buffer, since the data can only get smaller and we don't want to internally allocate memory if we
            /// can avoid it. It returns the length of the result.
            virtual AZ::u64 ConvertToAlias(char* inOutBuffer, AZ::u64 bufferLength) const = 0;

            /// ResolvePath - Replaces any aliases in path with their values and stores the result in resolvedPath,
            /// also ensures that the path is absolute
            /// returns true if path was resolved, false otherwise
            /// note that all of the above file-finding and opening functions automatically resolve the path before operating
            /// so you should not need to call this except in very exceptional circumstances where you absolutely need to
            /// hit a physical file and don't want to use SystemFile
            virtual bool ResolvePath(const char* path, char* resolvedPath, AZ::u64 resolvedPathSize) = 0;

            /// Divulge the filname used to originally open that handle.
            virtual bool GetFilename(HandleType fileHandle, char* filename, AZ::u64 filenameSize) const = 0;

            virtual bool IsRemoteIOEnabled()
            {
                return false;
            }
        };

        /**
         * Stream implementation for reading/writing to/from a FileIO handle.
         * This may be used alongside ObjectStream, or in async asset tasks.
         */
        class FileIOStream
            : public GenericStream
        {
        public:
            AZ_CLASS_ALLOCATOR(FileIOStream, SystemAllocator, 0);
            FileIOStream();
            FileIOStream(HandleType fileHandle, AZ::IO::OpenMode mode, bool ownsHandle);
            FileIOStream(const char* path, AZ::IO::OpenMode mode);
            ~FileIOStream() override;

            bool Open(const char* path, AZ::IO::OpenMode mode);
            bool ReOpen() override;
            void Close() override;
            HandleType GetHandle() const;
            const char* GetFilename() const override;
            OpenMode GetModeFlags() const override;

            bool        IsOpen() const override;
            bool        CanSeek() const override;
            bool        CanRead() const override;
            bool        CanWrite() const override;
            void        Seek(OffsetType bytes, SeekMode mode) override;
            SizeType    Read(SizeType bytes, void* oBuffer) override;
            SizeType    Write(SizeType bytes, const void* iBuffer) override;
            SizeType    GetCurPos() const override;
            SizeType    GetLength() const override;

        private:
            HandleType m_handle;    ///< Open file handle.
            bool m_ownsHandle;      ///< Set if the FileIOStream instance created the handle (and should therefore auto-close it on destruction).
            AZStd::string m_filename; ///< Stores filename for reopen support
            OpenMode m_mode; ///< Stores open mode flag to be used when reopening
        };
    } // namespace IO
} // namespace AZ
#endif // #ifndef AZCORE_IO_FILEIO_H
