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

#include <AzCore/Serialization/ObjectStream.h>
#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/std/string/string.h>

namespace AZ
{
    namespace IO
    {
        class GenericStream;
    }

    namespace Utils
    {
        typedef AZ::ObjectStream::FilterDescriptor FilterDescriptor;

        void* LoadObjectFromStream(IO::GenericStream& stream, SerializeContext* context = nullptr, const Uuid* targetClassId = nullptr, const FilterDescriptor& filterDesc = FilterDescriptor());
        bool LoadObjectFromStreamInPlace(IO::GenericStream& stream, AZ::SerializeContext* context, const Uuid& targetClassId, void* targetPointer, const FilterDescriptor& filterDesc = FilterDescriptor());
        bool LoadObjectFromStreamInPlace(IO::GenericStream& stream, AZ::SerializeContext* context, const SerializeContext::ClassData* objectClassData, void* targetPointer, const FilterDescriptor& filterDesc = FilterDescriptor());

        template <typename ObjectType>
        ObjectType* LoadObjectFromStream(IO::GenericStream& stream, SerializeContext* context = nullptr, const FilterDescriptor& filterDesc = FilterDescriptor())
        {
            return reinterpret_cast<ObjectType*>(LoadObjectFromStream(stream, context, &AzTypeInfo<ObjectType>::Uuid(), filterDesc));
        }

        template <typename ObjectType>
        ObjectType* LoadObjectFromBuffer(const void* buffer, const AZStd::size_t length, SerializeContext* context = nullptr, const FilterDescriptor& filterDesc = FilterDescriptor())
        {
            AZ::IO::MemoryStream stream(buffer, length);
            return LoadObjectFromStream<ObjectType>(stream, context, filterDesc);
        }

        void* LoadObjectFromFile(const AZStd::string& filePath, const Uuid& targetClassId, SerializeContext* context = nullptr, const FilterDescriptor& filterDesc = FilterDescriptor(), int platformFlags = 0);

        template <typename ObjectType>
        ObjectType* LoadObjectFromFile(const AZStd::string& filePath, SerializeContext* context = nullptr, const FilterDescriptor& filterDesc = FilterDescriptor(), int platformFlags = 0)
        {
            return reinterpret_cast<ObjectType*>(LoadObjectFromFile(filePath, AzTypeInfo<ObjectType>::Uuid(), context, filterDesc, platformFlags));
        }

        bool SaveObjectToStream(IO::GenericStream& stream, DataStream::StreamType streamType, const void* classPtr, const Uuid& classId, SerializeContext* context = nullptr, const SerializeContext::ClassData* classData = nullptr);

        template <typename ObjectType>
        bool SaveObjectToStream(IO::GenericStream& stream, DataStream::StreamType streamType, const ObjectType* classPtr, SerializeContext* context = nullptr, const SerializeContext::ClassData* classData = nullptr)
        {
            return SaveObjectToStream(stream, streamType, classPtr, AzTypeInfo<ObjectType>::Uuid(), context, classData);
        }

        bool SaveObjectToFile(const AZStd::string& filePath, DataStream::StreamType fileType, const void* classPtr, const Uuid& classId, SerializeContext* context = nullptr, int platformFlags = 0);

        template <typename ObjectType>
        bool SaveObjectToFile(const AZStd::string& filePath, DataStream::StreamType fileType, const ObjectType* classPtr, SerializeContext* context = nullptr, int platformFlags = 0)
        {
            return SaveObjectToFile(filePath, fileType, classPtr, AzTypeInfo<ObjectType>::Uuid(), context, platformFlags);
        }

        template <typename ObjectType>
        bool LoadObjectFromStreamInPlace(IO::GenericStream& stream, ObjectType& destination, AZ::SerializeContext* context = nullptr, const FilterDescriptor& filterDesc = FilterDescriptor())
        {
            return LoadObjectFromStreamInPlace(stream, context, AzTypeInfo<ObjectType>::Uuid(), &destination, filterDesc);
        }

        template <typename ObjectType>
        bool LoadObjectFromBufferInPlace(const void* buffer, const AZStd::size_t length, ObjectType& destination, AZ::SerializeContext* context = nullptr, const FilterDescriptor& filterDesc = FilterDescriptor())
        {
            AZ::IO::MemoryStream stream(buffer, length);
            return LoadObjectFromStreamInPlace(stream, context, AzTypeInfo<ObjectType>::Uuid(), &destination, filterDesc);
        }

        AZStd::vector<AZ::SerializeContext::DataElementNode*> FindDescendantElements(AZ::SerializeContext& context, AZ::SerializeContext::DataElementNode& classElement, const AZStd::vector<AZ::Crc32>& elementCrcQueue);
        void FindDescendantElements(AZ::SerializeContext& context, AZ::SerializeContext::DataElementNode& classElement, AZStd::vector<AZ::SerializeContext::DataElementNode*>& dataElementNodes, AZStd::vector<AZ::Crc32>::const_iterator first, AZStd::vector<AZ::Crc32>::const_iterator last);
        bool LoadObjectFromFileInPlace(const AZStd::string& filePath, const Uuid& targetClassId, void* destination, AZ::SerializeContext* context = nullptr, const FilterDescriptor& filterDesc = FilterDescriptor());

        template <typename ObjectType>
        bool LoadObjectFromFileInPlace(const AZStd::string& filePath, ObjectType& destination, AZ::SerializeContext* context = nullptr, const FilterDescriptor& filterDesc = FilterDescriptor())
        {
            return LoadObjectFromFileInPlace(filePath, AzTypeInfo<ObjectType>::Uuid(), &destination, context, filterDesc);
        }
        
        bool IsVectorContainerType(const AZ::Uuid& type);
        bool IsSetContainerType(const AZ::Uuid& type);
        bool IsMapContainerType(const AZ::Uuid& type);
        bool IsContainerType(const AZ::Uuid& type);
        bool IsOutcomeType(const AZ::Uuid& type);

        AZ::TypeId GetGenericContainerType(const AZ::TypeId& type);
        bool IsGenericContainerType(const AZ::TypeId& type);
        AZStd::pair<AZ::Uuid, AZ::Uuid> GetOutcomeTypes(const AZ::Uuid& type);
        AZStd::vector<AZ::Uuid> GetContainedTypes(const AZ::Uuid& type);

    } // namespace Utils
} // namespace AzCore
