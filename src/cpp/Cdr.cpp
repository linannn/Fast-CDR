// Copyright 2016 Proyectos y Sistemas de Mantenimiento SL (eProsima).
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <fastcdr/Cdr.h>
#include <fastcdr/exceptions/BadParamException.h>

using namespace eprosima::fastcdr;
using namespace ::exception;

#if __BIG_ENDIAN__//获取字节顺序 大端模式或小端模式
const Cdr::Endianness Cdr::DEFAULT_ENDIAN = BIG_ENDIANNESS;
#else
const Cdr::Endianness Cdr::DEFAULT_ENDIAN = LITTLE_ENDIANNESS;
#endif

CONSTEXPR size_t ALIGNMENT_LONG_DOUBLE = 8;

Cdr::state::state(const Cdr &cdr) : m_currentPosition(cdr.m_currentPosition), m_alignPosition(cdr.m_alignPosition),
    m_swapBytes(cdr.m_swapBytes), m_lastDataSize(cdr.m_lastDataSize) {}

    Cdr::state::state(const state &state) : m_currentPosition(state.m_currentPosition), m_alignPosition(state.m_alignPosition),
    m_swapBytes(state.m_swapBytes), m_lastDataSize(state.m_lastDataSize) {}

Cdr::Cdr(FastBuffer &cdrBuffer, const Endianness endianness, const CdrType cdrType) : m_cdrBuffer(cdrBuffer),
    m_cdrType(cdrType), m_plFlag(DDS_CDR_WITHOUT_PL), m_options(0), m_endianness((uint8_t)endianness),
    m_swapBytes(endianness == DEFAULT_ENDIAN ? false : true), m_lastDataSize(0), m_currentPosition(cdrBuffer.begin()),
    m_alignPosition(cdrBuffer.begin()), m_lastPosition(cdrBuffer.end())
{
}

Cdr& Cdr::read_encapsulation()
{
    uint8_t dummy = 0, encapsulationKind = 0;
    state state(*this);

    try
    {
        // If it is DDS_CDR, the first step is to get the dummy byte.
        if(m_cdrType == DDS_CDR)
        {
            (*this) >> dummy;
        }

        // Get the ecampsulation byte.
        (*this) >> encapsulationKind;


        // If it is a different endianness, make changes.
        if(m_endianness != (encapsulationKind & 0x1))
        {
            m_swapBytes = !m_swapBytes;
            m_endianness = (encapsulationKind & 0x1);
        }
    }
    catch(Exception &ex)
    {
        setState(state);
        ex.raise();
    }

    // If it is DDS_CDR type, view if contains a parameter list.
    if(encapsulationKind & DDS_CDR_WITH_PL)
    {
        if(m_cdrType == DDS_CDR)
        {
            m_plFlag = DDS_CDR_WITH_PL;
        }
        else
        {
            throw BadParamException("Unexpected CDR type received in Cdr::read_encapsulation");
        }
    }

    try
    {
        if(m_cdrType == DDS_CDR)
            (*this) >> m_options;
    }
    catch(Exception &ex)
    {
        setState(state);
        ex.raise();
    }

    resetAlignment();
    return *this;
}

Cdr& Cdr::serialize_encapsulation()
{
    uint8_t dummy = 0, encapsulationKind = 0;
    state state(*this);

    try
    {
        // If it is DDS_CDR, the first step is to serialize the dummy byte.
        if(m_cdrType == DDS_CDR)
        {
            (*this) << dummy;
        }

        // Construct encapsulation byte.
        encapsulationKind = ((uint8_t)m_plFlag | m_endianness);

        // Serialize the encapsulation byte.
        (*this) << encapsulationKind;
    }
    catch(Exception &ex)
    {
        setState(state);
        ex.raise();
    }

    try
    {
        if(m_cdrType == DDS_CDR)
            (*this) << m_options;
    }
    catch(Exception &ex)
    {
        setState(state);
        ex.raise();
    }

    resetAlignment();
    return *this;
}

Cdr::DDSCdrPlFlag Cdr::getDDSCdrPlFlag() const
{
    return m_plFlag;
}

void Cdr::setDDSCdrPlFlag(DDSCdrPlFlag plFlag)
{
    m_plFlag = plFlag;
}

uint16_t Cdr::getDDSCdrOptions() const
{
    return m_options;
}

void Cdr::setDDSCdrOptions(uint16_t options)
{
    m_options = options;
}

void Cdr::changeEndianness(Endianness endianness)
{
    if(m_endianness != endianness)
    {
        m_swapBytes = !m_swapBytes;
        m_endianness = endianness;
    }
}

bool Cdr::jump(size_t numBytes)
{
    bool returnedValue = false;

    if(((m_lastPosition - m_currentPosition) >= numBytes) || resize(numBytes))
    {
        m_currentPosition += numBytes;
        returnedValue = true;
    }

    return returnedValue;
}

char* Cdr::getBufferPointer()
{
    return m_cdrBuffer.getBuffer();
}

char* Cdr::getCurrentPosition()
{
    return &m_currentPosition;
}

Cdr::state Cdr::getState()
{
    return Cdr::state(*this);
}

void Cdr::setState(state &state)
{
    m_currentPosition >> state.m_currentPosition;
    m_alignPosition >> state.m_alignPosition;
    m_swapBytes = state.m_swapBytes;
    m_lastDataSize = state.m_lastDataSize;
}

void Cdr::reset()
{
    m_currentPosition = m_cdrBuffer.begin();
    m_alignPosition = m_cdrBuffer.begin();
    m_swapBytes = m_endianness == DEFAULT_ENDIAN ? false : true;
    m_lastDataSize = 0;
}

bool Cdr::moveAlignmentForward(size_t numBytes)
{
    bool returnedValue = false;

    if(((m_lastPosition - m_alignPosition) >= numBytes) || resize(numBytes))
    {
        m_alignPosition += numBytes;
        returnedValue = true;
    }

    return returnedValue;
}

bool Cdr::resize(size_t minSizeInc)
{
    if(m_cdrBuffer.resize(minSizeInc))
    {
        m_currentPosition << m_cdrBuffer.begin();
        m_alignPosition << m_cdrBuffer.begin();
        m_lastPosition = m_cdrBuffer.end();
        return true;
    }

    return false;
}

Cdr& Cdr::serialize(const char char_t)
{
    if(((m_lastPosition - m_currentPosition) >= sizeof(char_t)) || resize(sizeof(char_t)))
    {
        // Save last datasize.
        m_lastDataSize = sizeof(char_t);

        m_currentPosition++ << char_t;
        return *this;
    }

    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

Cdr& Cdr::serialize(const int16_t short_t)
{
    size_t align = alignment(sizeof(short_t));
    size_t sizeAligned = sizeof(short_t) + align;

    if(((m_lastPosition - m_currentPosition) >= sizeAligned) || resize(sizeAligned))
    {
        // Save last datasize.
        m_lastDataSize = sizeof(short_t);

        // Align.
        makeAlign(align);

        if(m_swapBytes)
        {
            const char *dst = reinterpret_cast<const char*>(&short_t);

            m_currentPosition++ << dst[1];
            m_currentPosition++ << dst[0];
        }
        else
        {
            m_currentPosition << short_t;
            m_currentPosition += sizeof(short_t);
        }

        return *this;
    }

    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

Cdr& Cdr::serialize(const int16_t short_t, Endianness endianness)
{
    bool auxSwap = m_swapBytes;
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        serialize(short_t);
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

Cdr& Cdr::serialize(const int32_t long_t)
{
    size_t align = alignment(sizeof(long_t));
    size_t sizeAligned = sizeof(long_t) + align;

    if(((m_lastPosition - m_currentPosition) >= sizeAligned) || resize(sizeAligned))
    {
        // Save last datasize.
        m_lastDataSize = sizeof(long_t);

        // Align.
        makeAlign(align);

        if(m_swapBytes)
        {
            const char *dst = reinterpret_cast<const char*>(&long_t);

            m_currentPosition++ << dst[3];
            m_currentPosition++ << dst[2];
            m_currentPosition++ << dst[1];
            m_currentPosition++ << dst[0];
        }
        else
        {
            m_currentPosition << long_t;
            m_currentPosition += sizeof(long_t);
        }

        return *this;
    }

    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

Cdr& Cdr::serialize(const int32_t long_t, Endianness endianness)
{
    bool auxSwap = m_swapBytes;
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        serialize(long_t);
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

Cdr& Cdr::serialize(const int64_t longlong_t)
{
    size_t align = alignment(sizeof(longlong_t));
    size_t sizeAligned = sizeof(longlong_t) + align;

    if(((m_lastPosition - m_currentPosition) >= sizeAligned) || resize(sizeAligned))
    {
        // Save last datasize.
        m_lastDataSize = sizeof(longlong_t);

        // Align.
        makeAlign(align);

        if(m_swapBytes)
        {
            const char *dst = reinterpret_cast<const char*>(&longlong_t);

            m_currentPosition++ << dst[7];
            m_currentPosition++ << dst[6];
            m_currentPosition++ << dst[5];
            m_currentPosition++ << dst[4];
            m_currentPosition++ << dst[3];
            m_currentPosition++ << dst[2];
            m_currentPosition++ << dst[1];
            m_currentPosition++ << dst[0];
        }
        else
        {
            m_currentPosition << longlong_t;
            m_currentPosition += sizeof(longlong_t);
        }

        return *this;
    }

    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

Cdr& Cdr::serialize(const int64_t longlong_t, Endianness endianness)
{
    bool auxSwap = m_swapBytes;
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        serialize(longlong_t);
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

Cdr& Cdr::serialize(const float float_t)
{
    size_t align = alignment(sizeof(float_t));
    size_t sizeAligned = sizeof(float_t) + align;

    if(((m_lastPosition - m_currentPosition) >= sizeAligned) || resize(sizeAligned))
    {
        // Save last datasize.
        m_lastDataSize = sizeof(float_t);

        // Align.
        makeAlign(align);

        if(m_swapBytes)
        {
            const char *dst = reinterpret_cast<const char*>(&float_t);

            m_currentPosition++ << dst[3];
            m_currentPosition++ << dst[2];
            m_currentPosition++ << dst[1];
            m_currentPosition++ << dst[0];
        }
        else
        {
            m_currentPosition << float_t;
            m_currentPosition += sizeof(float_t);
        }

        return *this;
    }

    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

Cdr& Cdr::serialize(const float float_t, Endianness endianness)
{
    bool auxSwap = m_swapBytes;
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        serialize(float_t);
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

Cdr& Cdr::serialize(const double double_t)
{
    size_t align = alignment(sizeof(double_t));
    size_t sizeAligned = sizeof(double_t) + align;

    if(((m_lastPosition - m_currentPosition) >= sizeAligned) || resize(sizeAligned))
    {
        // Save last datasize.
        m_lastDataSize = sizeof(double_t);

        // Align.
        makeAlign(align);

        if(m_swapBytes)
        {
            const char *dst = reinterpret_cast<const char*>(&double_t);

            m_currentPosition++ << dst[7];
            m_currentPosition++ << dst[6];
            m_currentPosition++ << dst[5];
            m_currentPosition++ << dst[4];
            m_currentPosition++ << dst[3];
            m_currentPosition++ << dst[2];
            m_currentPosition++ << dst[1];
            m_currentPosition++ << dst[0];
        }
        else
        {
            m_currentPosition << double_t;
            m_currentPosition += sizeof(double_t);
        }

        return *this;
    }

    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}
//  linann note start
// alignment函数？？？？？？？
// @brief 该函数用于序列化有不同字节顺序的double型变量
// @param double_t 将在缓冲区中序列化的double变量的值
// @param endianness 在此double型变量序列化过程中所用的字节顺序
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception exception:: NotEnoughMemoryException尝试序列化超过内部存储器大小的位置时引发此异常。
Cdr& Cdr::serialize(const double double_t, Endianness endianness)
{   
    //临时变量,用于记录m_swapBytes
    bool auxSwap = m_swapBytes;
    //根据参数endianness以及机器字节存储顺序确定是否需要交换字节
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        //调用serialize(double double_t)函数
        serialize(double_t);
        //还原m_swapBytes
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        //抛出异常同时还原m_swapBytes
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

// @brief 该函数用于在机器的默认字节顺序下序列化一个long double变量
// @param ldouble_t 将在缓冲区中序列化的ldouble变量的值
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception exception:: NotEnoughMemoryException尝试序列化超过内部存储器大小的位置时引发此异常。
Cdr& Cdr::serialize(const long double ldouble_t)
{
    //需要对齐的字节数
    size_t align = alignment(ALIGNMENT_LONG_DOUBLE);
    //序列化该数据类型所需的空间大小包括对齐加自身的大小
    size_t sizeAligned = sizeof(ldouble_t) + align;

    //判断当前缓冲区大小是否大于序列化所需空间，或者是否可以申请到相应大小的内存
    if(((m_lastPosition - m_currentPosition) >= sizeAligned) || resize(sizeAligned))
    {
        //存储最新的数据大小i
        // Save last datasize.
        m_lastDataSize = sizeof(ldouble_t);

        //数据对齐
        // Align.
        makeAlign(align);

        //判断是否需要交换字节
        if(m_swapBytes)
        {   
            //如果需要交换字节，将该long double型变量转换成char数组
            //通过将该变量取地址后把该地址强制转换成一个char型制指针实现
            const char *dst = reinterpret_cast<const char*>(&ldouble_t);

            //按从后向前的顺序将long double型数据的每个字节写入缓冲区并同时改变m_currentPosition的位置
            m_currentPosition++ << dst[15];
            m_currentPosition++ << dst[14];
            m_currentPosition++ << dst[13];
            m_currentPosition++ << dst[12];
            m_currentPosition++ << dst[11];
            m_currentPosition++ << dst[10];
            m_currentPosition++ << dst[9];
            m_currentPosition++ << dst[8];
            m_currentPosition++ << dst[7];
            m_currentPosition++ << dst[6];
            m_currentPosition++ << dst[5];
            m_currentPosition++ << dst[4];
            m_currentPosition++ << dst[3];
            m_currentPosition++ << dst[2];
            m_currentPosition++ << dst[1];
            m_currentPosition++ << dst[0];
        }
        else
        {
            //如果不需要交换字节，则直接将该数据写入缓冲区
            m_currentPosition << ldouble_t;
            //同时m_currentPosition的位置后移long double大小
            m_currentPosition += sizeof(ldouble_t);
        }

        return *this;
    }

    //抛出异常
    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

// @brief 该函数用于序列化有不同字节顺序的long double型变量
// @param ldouble_t 将在缓冲区中序列化的long double变量的值
// @param endianness 在此double型变量序列化过程中所用的字节顺序
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception exception:: NotEnoughMemoryException尝试序列化超过内部存储器大小的位置时引发此异常。
Cdr& Cdr::serialize(const long double ldouble_t, Endianness endianness)
{
    //临时变量，用于记录m_swapBytes
    bool auxSwap = m_swapBytes;
    //根据参数endianness以及机器的字节顺序确定最终的字节顺序
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        //调用serialize(long double ldouble_t)函数
        serialize(ldouble_t);
        //还原m_swapBytes
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        //抛出异常同时还原m_swapBytes
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

// @brief 该函数用于在机器的默认字节顺序下序列化一个long double变量
// @param ldouble_t 将在缓冲区中序列化的ldouble变量的值
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception exception:: NotEnoughMemoryException尝试序列化超过内部存储器大小的位置时引发此异常。
Cdr& Cdr::serialize(const bool bool_t)
{
    //定义一个unsigned char临时变量用于序列化该bool型变量并给其赋初值0
    uint8_t value = 0;

  //判断当前缓冲区大小是否大于序列化所需空间，或者是否可以申请到相应大小的内存
    if(((m_lastPosition - m_currentPosition) >= sizeof(uint8_t)) || resize(sizeof(uint8_t)))
    {
        //保存最新的数据大小
        // Save last datasize.
        m_lastDataSize = sizeof(uint8_t);

        //如果该bool型变量为true，把临时变量value置为1
        if(bool_t)
            value = 1;
        //把临时变量写入缓冲区
        m_currentPosition++ << value;

        return *this;
    }

    //如果条件不满足则抛出异常
    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

// @brief 该函数用于在机器的默认字节顺序下序列化一个字符串 即String
// @param string_t是一个指向将被序列化的字符串的指针
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception exception:: NotEnoughMemoryException尝试序列化超过内部存储器大小的位置时引发此异常。
Cdr& Cdr::serialize(const char *string_t)
{
    //unsigned int型临时变量，用于存储字符串长度
    uint32_t length = 0;

    //获取字符串的长度
    if(string_t != nullptr)
        length = (uint32_t)strlen(string_t) + 1;

    if(length > 0)
    {
        //如果字符串不为空

        //复制当前cdr序列化的状态
        Cdr::state state(*this);
        //首先将字符串的长度序列化
        serialize(length);

         //判断当前缓冲区大小是否大于序列化所需空间，或者是否可以申请到相应大小的内存
        if(((m_lastPosition - m_currentPosition) >= length) || resize(length))
        {
            //存储最新的数据大小
            // Save last datasize.
            m_lastDataSize = sizeof(uint8_t);

            //将字符串拷贝到缓冲区
            m_currentPosition.memcopy(string_t, length);
            //将m_currentPostion向后移动length
            m_currentPosition += length;
        }
        else
        {
            //如果没能成功序列化则使用前面复制的state重置CDR序列化的状态，并抛出异常
            setState(state);
            throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
        }
    }
    else
        //如果字符串为空则仅序列化其长度
        serialize(length);

    return *this;
}

// @brief 该函数用于根据给定的字节顺序序列化字符串，即String
// @param string_t 是一个指向将被序列化的字符串的指针
// @param endianness 在此字符串序列化过程中所用的字节顺序
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception exception:: NotEnoughMemoryException尝试序列化超过内部存储器大小的位置时引发此异常。
Cdr& Cdr::serialize(const char *string_t, Endianness endianness)
{

    //临时变量，用于记录m_swapBytes
    bool auxSwap = m_swapBytes;
    //根据参数endianness以及机器字节存储顺序确定是否需要交换字节
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        //调用serialize(char *string_t)
        serialize(string_t);
        //还原m_swapBytes
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        //抛出异常同时还原m_swapBytes
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

// @brief 该函数序列化一个bool型数组
// @param bool_t 将要序列化的bool型数组
// @param numElements bool型数组元素的数量
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception exception:: NotEnoughMemoryException尝试序列化超过内部存储器大小的位置时引发此异常。
Cdr& Cdr::serializeArray(const bool *bool_t, size_t numElements)
{
    //该bool数组的总大小
    size_t totalSize = sizeof(*bool_t)*numElements;

  //判断当前缓冲区大小是否大于序列化所需空间，或者是否可以申请到相应大小的内存
    if(((m_lastPosition - m_currentPosition) >= totalSize) || resize(totalSize))
    {
        //保存最新的数据大小，注意是bool型的大小而不是数组大小
        // Save last datasize.
        m_lastDataSize = sizeof(*bool_t);

        //循环将bool数组中的元素写入缓冲区，方法与序列化bool型变量的方法完全相同
        for(size_t count = 0; count < numElements; ++count)
        {
            uint8_t value = 0;

            if(bool_t[count])
                value = 1;
            m_currentPosition++ << value;
        }

        return *this;
    }

    //如果条件不满足则抛出异常
    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

// @brief 该函数序列化一个字符数组
// @param char_t 将要序列化的字符数组
// @param numElements bool型数组元素的数量
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception exception:: NotEnoughMemoryException尝试序列化超过内部存储器大小的位置时引发此异常。
Cdr& Cdr::serializeArray(const char *char_t, size_t numElements)
{
    //该字符数组的总大小
    size_t totalSize = sizeof(*char_t)*numElements;

  //判断当前缓冲区大小是否大于序列化所需空间，或者是否可以申请到相应大小的内存
    if(((m_lastPosition - m_currentPosition) >= totalSize) || resize(totalSize))
    {
        //保存最新的数据大小，即char型
        // Save last datasize.
        m_lastDataSize = sizeof(*char_t);

        //将chat_t拷贝到缓冲区中
        m_currentPosition.memcopy(char_t, totalSize);
        //更新缓冲区m_currentPosition的位置
        m_currentPosition += totalSize;
        return *this;
    }

    //条件不满足则抛出异常
    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

// @brief 该函数序列化一个short数组
// @param short_t 将要序列化的short数组
// @param numElements short数组元素的数量
// @return 一个eprosima::fastcdr::Cdr对象的引用
// @exception exception:: NotEnoughMemoryException尝试序列化超过内部存储器大小的位置时引发此异常。
Cdr& Cdr::serializeArray(const int16_t *short_t, size_t numElements)
{
    size_t align = alignment(sizeof(*short_t));
    size_t totalSize = sizeof(*short_t) * numElements;
    size_t sizeAligned = totalSize + align;

    if(((m_lastPosition - m_currentPosition) >= sizeAligned) || resize(sizeAligned))
    {
        // Save last datasize.
        m_lastDataSize = sizeof(*short_t);

        // Align if there are any elements
        if(numElements)
            makeAlign(align);

        if(m_swapBytes)
        {
            const char *dst = reinterpret_cast<const char*>(&short_t);
            const char *end = dst + totalSize;

            for(; dst < end; dst += sizeof(*short_t))
            {
                m_currentPosition++ << dst[1];
                m_currentPosition++ << dst[0];
            }
        }
        else
        {
            m_currentPosition.memcopy(short_t, totalSize);
            m_currentPosition += totalSize;
        }

        return *this;
    }

    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

Cdr& Cdr::serializeArray(const int16_t *short_t, size_t numElements, Endianness endianness)
{
    bool auxSwap = m_swapBytes;
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        serializeArray(short_t, numElements);
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

Cdr& Cdr::serializeArray(const int32_t *long_t, size_t numElements)
{
    size_t align = alignment(sizeof(*long_t));
    size_t totalSize = sizeof(*long_t) * numElements;
    size_t sizeAligned = totalSize + align;

    if(((m_lastPosition - m_currentPosition) >= sizeAligned) || resize(sizeAligned))
    {
        // Save last datasize.
        m_lastDataSize = sizeof(*long_t);

        // Align if there are any elements
        if(numElements)
            makeAlign(align);

        if(m_swapBytes)
        {
            const char *dst = reinterpret_cast<const char*>(&long_t);
            const char *end = dst + totalSize;

            for(; dst < end; dst += sizeof(*long_t))
            {
                m_currentPosition++ << dst[3];
                m_currentPosition++ << dst[2];
                m_currentPosition++ << dst[1];
                m_currentPosition++ << dst[0];
            }
        }
        else
        {
            m_currentPosition.memcopy(long_t, totalSize);
            m_currentPosition += totalSize;
        }

        return *this;
    }

    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

Cdr& Cdr::serializeArray(const int32_t *long_t, size_t numElements, Endianness endianness)
{
    bool auxSwap = m_swapBytes;
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        serializeArray(long_t, numElements);
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

Cdr& Cdr::serializeArray(const wchar_t *wchar, size_t numElements)
{
    for(size_t count = 0; count < numElements; ++count)
        serialize(wchar[count]);
    return *this;
}

Cdr& Cdr::serializeArray(const wchar_t *wchar, size_t numElements, Endianness endianness)
{
    bool auxSwap = m_swapBytes;
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        serializeArray(wchar, numElements);
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

Cdr& Cdr::serializeArray(const int64_t *longlong_t, size_t numElements)
{
    size_t align = alignment(sizeof(*longlong_t));
    size_t totalSize = sizeof(*longlong_t) * numElements;
    size_t sizeAligned = totalSize + align;

    if(((m_lastPosition - m_currentPosition) >= sizeAligned) || resize(sizeAligned))
    {
        // Save last datasize.
        m_lastDataSize = sizeof(*longlong_t);

        // Align if there are any elements
        if(numElements)
            makeAlign(align);

        if(m_swapBytes)
        {
            const char *dst = reinterpret_cast<const char*>(&longlong_t);
            const char *end = dst + totalSize;

            for(; dst < end; dst += sizeof(*longlong_t))
            {
                m_currentPosition++ << dst[7];
                m_currentPosition++ << dst[6];
                m_currentPosition++ << dst[5];
                m_currentPosition++ << dst[4];
                m_currentPosition++ << dst[3];
                m_currentPosition++ << dst[2];
                m_currentPosition++ << dst[1];
                m_currentPosition++ << dst[0];
            }
        }
        else
        {
            m_currentPosition.memcopy(longlong_t, totalSize);
            m_currentPosition += totalSize;
        }

        return *this;
    }

    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

Cdr& Cdr::serializeArray(const int64_t *longlong_t, size_t numElements, Endianness endianness)
{
    bool auxSwap = m_swapBytes;
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        serializeArray(longlong_t, numElements);
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

Cdr& Cdr::serializeArray(const float *float_t, size_t numElements)
{
    size_t align = alignment(sizeof(*float_t));
    size_t totalSize = sizeof(*float_t) * numElements;
    size_t sizeAligned = totalSize + align;

    if(((m_lastPosition - m_currentPosition) >= sizeAligned) || resize(sizeAligned))
    {
        // Save last datasize.
        m_lastDataSize = sizeof(*float_t);

        // Align if there are any elements
        if(numElements)
            makeAlign(align);

        if(m_swapBytes)
        {
            const char *dst = reinterpret_cast<const char*>(&float_t);
            const char *end = dst + totalSize;

            for(; dst < end; dst += sizeof(*float_t))
            {
                m_currentPosition++ << dst[3];
                m_currentPosition++ << dst[2];
                m_currentPosition++ << dst[1];
                m_currentPosition++ << dst[0];
            }
        }
        else
        {
            m_currentPosition.memcopy(float_t, totalSize);
            m_currentPosition += totalSize;
        }

        return *this;
    }

    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

Cdr& Cdr::serializeArray(const float *float_t, size_t numElements, Endianness endianness)
{
    bool auxSwap = m_swapBytes;
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        serializeArray(float_t, numElements);
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

Cdr& Cdr::serializeArray(const double *double_t, size_t numElements)
{
    size_t align = alignment(sizeof(*double_t));
    size_t totalSize = sizeof(*double_t) * numElements;
    size_t sizeAligned = totalSize + align;

    if(((m_lastPosition - m_currentPosition) >= sizeAligned) || resize(sizeAligned))
    {
        // Save last datasize.
        m_lastDataSize = sizeof(*double_t);

        // Align if there are any elements
        if(numElements)
            makeAlign(align);

        if(m_swapBytes)
        {
            const char *dst = reinterpret_cast<const char*>(&double_t);
            const char *end = dst + totalSize;

            for(; dst < end; dst += sizeof(*double_t))
            {
                m_currentPosition++ << dst[7];
                m_currentPosition++ << dst[6];
                m_currentPosition++ << dst[5];
                m_currentPosition++ << dst[4];
                m_currentPosition++ << dst[3];
                m_currentPosition++ << dst[2];
                m_currentPosition++ << dst[1];
                m_currentPosition++ << dst[0];
            }
        }
        else
        {
            m_currentPosition.memcopy(double_t, totalSize);
            m_currentPosition += totalSize;
        }

        return *this;
    }

    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

Cdr& Cdr::serializeArray(const double *double_t, size_t numElements, Endianness endianness)
{
    bool auxSwap = m_swapBytes;
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        serializeArray(double_t, numElements);
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}
// linann end  
Cdr& Cdr::serializeArray(const long double *ldouble_t, size_t numElements)
{
    size_t align = alignment(ALIGNMENT_LONG_DOUBLE);
    size_t totalSize = sizeof(*ldouble_t) * numElements;
    size_t sizeAligned = totalSize + align;

    if(((m_lastPosition - m_currentPosition) >= sizeAligned) || resize(sizeAligned))
    {
        // Save last datasize.
        m_lastDataSize = sizeof(*ldouble_t);

        // Align if there are any elements
        if(numElements)
            makeAlign(align);

        if(m_swapBytes)
        {
            const char *dst = reinterpret_cast<const char*>(&ldouble_t);
            const char *end = dst + totalSize;

            for(; dst < end; dst += sizeof(*ldouble_t))
            {
                m_currentPosition++ << dst[15];
                m_currentPosition++ << dst[14];
                m_currentPosition++ << dst[13];
                m_currentPosition++ << dst[12];
                m_currentPosition++ << dst[11];
                m_currentPosition++ << dst[10];
                m_currentPosition++ << dst[9];
                m_currentPosition++ << dst[8];
                m_currentPosition++ << dst[7];
                m_currentPosition++ << dst[6];
                m_currentPosition++ << dst[5];
                m_currentPosition++ << dst[4];
                m_currentPosition++ << dst[3];
                m_currentPosition++ << dst[2];
                m_currentPosition++ << dst[1];
                m_currentPosition++ << dst[0];
            }
        }
        else
        {
            m_currentPosition.memcopy(ldouble_t, totalSize);
            m_currentPosition += totalSize;
        }

        return *this;
    }

    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

Cdr& Cdr::serializeArray(const long double *ldouble_t, size_t numElements, Endianness endianness)
{
    bool auxSwap = m_swapBytes;
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        serializeArray(ldouble_t, numElements);
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

Cdr& Cdr::deserialize(char &char_t)
{
    if((m_lastPosition - m_currentPosition) >= sizeof(char_t))
    {
        // Save last datasize.
        m_lastDataSize = sizeof(char_t);

        m_currentPosition++ >> char_t;
        return *this;
    }

    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

Cdr& Cdr::deserialize(int16_t &short_t)
{
    size_t align = alignment(sizeof(short_t));
    size_t sizeAligned = sizeof(short_t) + align;

    if((m_lastPosition - m_currentPosition) >= sizeAligned)
    {
        // Save last datasize.
        m_lastDataSize = sizeof(short_t);

        // Align
        makeAlign(align);

        if(m_swapBytes)
        {    
            char *dst = reinterpret_cast<char*>(&short_t);

            m_currentPosition++ >> dst[1];
            m_currentPosition++ >> dst[0];
        }
        else
        {
            m_currentPosition >> short_t;
            m_currentPosition += sizeof(short_t);
        }

        return *this;
    }

    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

Cdr& Cdr::deserialize(int16_t &short_t, Endianness endianness)
{
    bool auxSwap = m_swapBytes;
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        deserialize(short_t);
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

Cdr& Cdr::deserialize(int32_t &long_t)
{
    size_t align = alignment(sizeof(long_t));
    size_t sizeAligned = sizeof(long_t) + align;

    if((m_lastPosition - m_currentPosition) >= sizeAligned)
    {
        // Save last datasize.
        m_lastDataSize = sizeof(long_t);

        // Align
        makeAlign(align);

        if(m_swapBytes)
        {
            char *dst = reinterpret_cast<char*>(&long_t);

            m_currentPosition++ >> dst[3];
            m_currentPosition++ >> dst[2];
            m_currentPosition++ >> dst[1];
            m_currentPosition++ >> dst[0];
        }
        else
        {
            m_currentPosition >> long_t;
            m_currentPosition += sizeof(long_t);
        }

        return *this;
    }

    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

Cdr& Cdr::deserialize(int32_t &long_t, Endianness endianness)
{
    bool auxSwap = m_swapBytes;
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        deserialize(long_t);
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

Cdr& Cdr::deserialize(int64_t &longlong_t)
{
    size_t align = alignment(sizeof(longlong_t));
    size_t sizeAligned = sizeof(longlong_t) + align;

    if((m_lastPosition - m_currentPosition) >= sizeAligned)
    {
        // Save last datasize.
        m_lastDataSize = sizeof(longlong_t);

        // Align.
        makeAlign(align);

        if(m_swapBytes)
        {
            char *dst = reinterpret_cast<char*>(&longlong_t);

            m_currentPosition++ >> dst[7];
            m_currentPosition++ >> dst[6];
            m_currentPosition++ >> dst[5];
            m_currentPosition++ >> dst[4];
            m_currentPosition++ >> dst[3];
            m_currentPosition++ >> dst[2];
            m_currentPosition++ >> dst[1];
            m_currentPosition++ >> dst[0];
        }
        else
        {
            m_currentPosition >> longlong_t;
            m_currentPosition += sizeof(longlong_t);
        }

        return *this;
    }

    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

Cdr& Cdr::deserialize(int64_t &longlong_t, Endianness endianness)
{
    bool auxSwap = m_swapBytes;
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        deserialize(longlong_t);
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

Cdr& Cdr::deserialize(float &float_t)
{
    size_t align = alignment(sizeof(float_t));
    size_t sizeAligned = sizeof(float_t) + align;

    if((m_lastPosition - m_currentPosition) >= sizeAligned)
    {
        // Save last datasize.
        m_lastDataSize = sizeof(float_t);

        // Align.
        makeAlign(align);

        if(m_swapBytes)
        {
            char *dst = reinterpret_cast<char*>(&float_t);

            m_currentPosition++ >> dst[3];
            m_currentPosition++ >> dst[2];
            m_currentPosition++ >> dst[1];
            m_currentPosition++ >> dst[0];
        }
        else
        {
            m_currentPosition >> float_t;
            m_currentPosition += sizeof(float_t);
        }

        return *this;
    }

    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

Cdr& Cdr::deserialize(float &float_t, Endianness endianness)
{
    bool auxSwap = m_swapBytes;
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        deserialize(float_t);
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

Cdr& Cdr::deserialize(double &double_t)
{
    size_t align = alignment(sizeof(double_t));
    size_t sizeAligned = sizeof(double_t) + align;

    if((m_lastPosition - m_currentPosition) >= sizeAligned)
    {
        // Save last datasize.
        m_lastDataSize = sizeof(double_t);

        // Align.
        makeAlign(align);

        if(m_swapBytes)
        {
            char *dst = reinterpret_cast<char*>(&double_t);

            m_currentPosition++ >> dst[7];
            m_currentPosition++ >> dst[6];
            m_currentPosition++ >> dst[5];
            m_currentPosition++ >> dst[4];
            m_currentPosition++ >> dst[3];
            m_currentPosition++ >> dst[2];
            m_currentPosition++ >> dst[1];
            m_currentPosition++ >> dst[0];
        }
        else
        {
            m_currentPosition >> double_t;
            m_currentPosition += sizeof(double_t);
        }

        return *this;
    }

    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

Cdr& Cdr::deserialize(double &double_t, Endianness endianness)
{
    bool auxSwap = m_swapBytes;
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        deserialize(double_t);
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

Cdr& Cdr::deserialize(long double &ldouble_t)
{
    size_t align = alignment(ALIGNMENT_LONG_DOUBLE);
    size_t sizeAligned = sizeof(ldouble_t) + align;

    if((m_lastPosition - m_currentPosition) >= sizeAligned)
    {
        // Save last datasize.
        m_lastDataSize = sizeof(ldouble_t);

        // Align.
        makeAlign(align);

        if(m_swapBytes)
        {
            char *dst = reinterpret_cast<char*>(&ldouble_t);

            m_currentPosition++ >> dst[7];
            m_currentPosition++ >> dst[6];
            m_currentPosition++ >> dst[5];
            m_currentPosition++ >> dst[4];
            m_currentPosition++ >> dst[3];
            m_currentPosition++ >> dst[2];
            m_currentPosition++ >> dst[1];
            m_currentPosition++ >> dst[0];
        }
        else
        {
            m_currentPosition >> ldouble_t;
            m_currentPosition += sizeof(ldouble_t);
        }

        return *this;
    }

    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

Cdr& Cdr::deserialize(long double &ldouble_t, Endianness endianness)
{
    bool auxSwap = m_swapBytes;
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        deserialize(ldouble_t);
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

Cdr& Cdr::deserialize(bool &bool_t)
{
    uint8_t value = 0;

    if((m_lastPosition - m_currentPosition) >= sizeof(uint8_t))
    {
        // Save last datasize.
        m_lastDataSize = sizeof(uint8_t);

        m_currentPosition++ >> value;

        if(value == 1)
        {
            bool_t = true;
            return *this;
        }
        else if(value == 0)
        {
            bool_t = false;
            return *this;
        }

        throw BadParamException("Unexpected byte value in Cdr::deserialize(bool), expected 0 or 1");
    }

    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

Cdr& Cdr::deserialize(char *&string_t)
{
    uint32_t length = 0;
    Cdr::state state(*this);

    deserialize(length);

    if(length == 0)
    {
        string_t = NULL;
        return *this;
    }
    else if((m_lastPosition - m_currentPosition) >= length)
    {
        // Save last datasize.
        m_lastDataSize = sizeof(uint8_t);

        // Allocate memory.
        string_t = (char*)calloc(length + ((&m_currentPosition)[length-1] == '\0' ? 0 : 1), sizeof(char));
        memcpy(string_t, &m_currentPosition, length);
        m_currentPosition += length;
        return *this;
    }

    setState(state);
    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

Cdr& Cdr::deserialize(char *&string_t, Endianness endianness)
{
    bool auxSwap = m_swapBytes;
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        deserialize(string_t);
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

const char* Cdr::readString(uint32_t &length)
{
    const char* returnedValue = "";
    state state(*this);

    *this >> length;

    if(length == 0)
    {
        return returnedValue;
    }
    else if((m_lastPosition - m_currentPosition) >= length)
    {
        // Save last datasize.
        m_lastDataSize = sizeof(uint8_t);

        returnedValue = &m_currentPosition;
        m_currentPosition += length;
        if(returnedValue[length-1] == '\0') --length;
        return returnedValue;
    }

    setState(state);
    throw eprosima::fastcdr::exception::NotEnoughMemoryException(eprosima::fastcdr::exception::NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

Cdr& Cdr::deserializeArray(bool *bool_t, size_t numElements)
{
    size_t totalSize = sizeof(*bool_t)*numElements;

    if((m_lastPosition - m_currentPosition) >= totalSize)
    {
        // Save last datasize.
        m_lastDataSize = sizeof(*bool_t);

        for(size_t count = 0; count < numElements; ++count)
        {
            uint8_t value = 0;
            m_currentPosition++ >> value;

            if(value == 1)
            {
                bool_t[count] = true;
            }
            else if(value == 0)
            {
                bool_t[count] = false;
            }
        }

        return *this;
    }

    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

Cdr& Cdr::deserializeArray(char *char_t, size_t numElements)
{
    size_t totalSize = sizeof(*char_t)*numElements;

    if((m_lastPosition - m_currentPosition) >= totalSize)
    {
        // Save last datasize.
        m_lastDataSize = sizeof(*char_t);

        m_currentPosition.rmemcopy(char_t, totalSize);
        m_currentPosition += totalSize;
        return *this;
    }

    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

Cdr& Cdr::deserializeArray(int16_t *short_t, size_t numElements)
{
    size_t align = alignment(sizeof(*short_t));
    size_t totalSize = sizeof(*short_t) * numElements;
    size_t sizeAligned = totalSize + align;

    if((m_lastPosition - m_currentPosition) >= sizeAligned)
    {
        // Save last datasize.
        m_lastDataSize = sizeof(*short_t);

        // Align if there are any elements
        if(numElements)
            makeAlign(align);

        if(m_swapBytes)
        {
            char *dst = reinterpret_cast<char*>(&short_t);
            char *end = dst + totalSize;

            for(; dst < end; dst += sizeof(*short_t))
            {
                m_currentPosition++ >> dst[1];
                m_currentPosition++ >> dst[0];
            }
        }
        else
        {
            m_currentPosition.rmemcopy(short_t, totalSize);
            m_currentPosition += totalSize;
        }

        return *this;
    }

    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

Cdr& Cdr::deserializeArray(int16_t *short_t, size_t numElements, Endianness endianness)
{
    bool auxSwap = m_swapBytes;
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        deserializeArray(short_t, numElements);
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

Cdr& Cdr::deserializeArray(int32_t *long_t, size_t numElements)
{
    size_t align = alignment(sizeof(*long_t));
    size_t totalSize = sizeof(*long_t) * numElements;
    size_t sizeAligned = totalSize + align;

    if((m_lastPosition - m_currentPosition) >= sizeAligned)
    {
        // Save last datasize.
        m_lastDataSize = sizeof(*long_t);

        // Align if there are any elements
        if(numElements)
            makeAlign(align);

        if(m_swapBytes)
        {
            char *dst = reinterpret_cast<char*>(&long_t);
            char *end = dst + totalSize;

            for(; dst < end; dst += sizeof(*long_t))
            {
                m_currentPosition++ >> dst[3];
                m_currentPosition++ >> dst[2];
                m_currentPosition++ >> dst[1];
                m_currentPosition++ >> dst[0];
            }
        }
        else
        {
            m_currentPosition.rmemcopy(long_t, totalSize);
            m_currentPosition += totalSize;
        }

        return *this;
    }

    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

Cdr& Cdr::deserializeArray(int32_t *long_t, size_t numElements, Endianness endianness)
{
    bool auxSwap = m_swapBytes;
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        deserializeArray(long_t, numElements);
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

Cdr& Cdr::deserializeArray(wchar_t *wchar, size_t numElements)
{
    uint32_t value;
    for(size_t count = 0; count < numElements; ++count)
    {
        deserialize(value);
        wchar[count] = (wchar_t)value;
    }
    return *this;
}

Cdr& Cdr::deserializeArray(wchar_t *wchar, size_t numElements, Endianness endianness)
{
    bool auxSwap = m_swapBytes;
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        deserializeArray(wchar, numElements);
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

Cdr& Cdr::deserializeArray(int64_t *longlong_t, size_t numElements)
{
    size_t align = alignment(sizeof(*longlong_t));
    size_t totalSize = sizeof(*longlong_t) * numElements;
    size_t sizeAligned = totalSize + align;

    if((m_lastPosition - m_currentPosition) >= sizeAligned)
    {
        // Save last datasize.
        m_lastDataSize = sizeof(*longlong_t);

        // Align if there are any elements
        if(numElements)
            makeAlign(align);

        if(m_swapBytes)
        {
            char *dst = reinterpret_cast<char*>(&longlong_t);
            char *end = dst + totalSize;

            for(; dst < end; dst += sizeof(*longlong_t))
            {
                m_currentPosition++ >> dst[7];
                m_currentPosition++ >> dst[6];
                m_currentPosition++ >> dst[5];
                m_currentPosition++ >> dst[4];
                m_currentPosition++ >> dst[3];
                m_currentPosition++ >> dst[2];
                m_currentPosition++ >> dst[1];
                m_currentPosition++ >> dst[0];
            }
        }
        else
        {
            m_currentPosition.rmemcopy(longlong_t, totalSize);
            m_currentPosition += totalSize;
        }

        return *this;
    }

    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

Cdr& Cdr::deserializeArray(int64_t *longlong_t, size_t numElements, Endianness endianness)
{
    bool auxSwap = m_swapBytes;
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        deserializeArray(longlong_t, numElements);
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

Cdr& Cdr::deserializeArray(float *float_t, size_t numElements)
{
    size_t align = alignment(sizeof(*float_t));
    size_t totalSize = sizeof(*float_t) * numElements;
    size_t sizeAligned = totalSize + align;

    if((m_lastPosition - m_currentPosition) >= sizeAligned)
    {
        // Save last datasize.
        m_lastDataSize = sizeof(*float_t);

        // Align if there are any elements
        if(numElements)
            makeAlign(align);

        if(m_swapBytes)
        {
            char *dst = reinterpret_cast<char*>(&float_t);
            char *end = dst + totalSize;

            for(; dst < end; dst += sizeof(*float_t))
            {
                m_currentPosition++ >> dst[3];
                m_currentPosition++ >> dst[2];
                m_currentPosition++ >> dst[1];
                m_currentPosition++ >> dst[0];
            }
        }
        else
        {
            m_currentPosition.rmemcopy(float_t, totalSize);
            m_currentPosition += totalSize;
        }

        return *this;
    }

    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

Cdr& Cdr::deserializeArray(float *float_t, size_t numElements, Endianness endianness)
{
    bool auxSwap = m_swapBytes;
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        deserializeArray(float_t, numElements);
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

Cdr& Cdr::deserializeArray(double *double_t, size_t numElements)
{
    size_t align = alignment(sizeof(*double_t));
    size_t totalSize = sizeof(*double_t) * numElements;
    size_t sizeAligned = totalSize + align;

    if((m_lastPosition - m_currentPosition) >= sizeAligned)
    {
        // Save last datasize.
        m_lastDataSize = sizeof(*double_t);

        // Align if there are any elements
        if(numElements)
            makeAlign(align);

        if(m_swapBytes)
        {
            char *dst = reinterpret_cast<char*>(&double_t);
            char *end = dst + totalSize;

            for(; dst < end; dst += sizeof(*double_t))
            {
                m_currentPosition++ >> dst[7];
                m_currentPosition++ >> dst[6];
                m_currentPosition++ >> dst[5];
                m_currentPosition++ >> dst[4];
                m_currentPosition++ >> dst[3];
                m_currentPosition++ >> dst[2];
                m_currentPosition++ >> dst[1];
                m_currentPosition++ >> dst[0];
            }
        }
        else
        {
            m_currentPosition.rmemcopy(double_t, totalSize);
            m_currentPosition += totalSize;
        }

        return *this;
    }

    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

Cdr& Cdr::deserializeArray(double *double_t, size_t numElements, Endianness endianness)
{
    bool auxSwap = m_swapBytes;
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        deserializeArray(double_t, numElements);
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

Cdr& Cdr::deserializeArray(long double *ldouble_t, size_t numElements)
{
    size_t align = alignment(ALIGNMENT_LONG_DOUBLE);
    size_t totalSize = sizeof(*ldouble_t) * numElements;
    size_t sizeAligned = totalSize + align;

    if((m_lastPosition - m_currentPosition) >= sizeAligned)
    {
        // Save last datasize.
        m_lastDataSize = sizeof(*ldouble_t);

        // Align if there are any elements
        if(numElements)
            makeAlign(align);

        if(m_swapBytes)
        {
            char *dst = reinterpret_cast<char*>(&ldouble_t);
            char *end = dst + totalSize;

            for(; dst < end; dst += sizeof(*ldouble_t))
            {
                m_currentPosition++ >> dst[7];
                m_currentPosition++ >> dst[6];
                m_currentPosition++ >> dst[5];
                m_currentPosition++ >> dst[4];
                m_currentPosition++ >> dst[3];
                m_currentPosition++ >> dst[2];
                m_currentPosition++ >> dst[1];
                m_currentPosition++ >> dst[0];
            }
        }
        else
        {
            m_currentPosition.rmemcopy(ldouble_t, totalSize);
            m_currentPosition += totalSize;
        }

        return *this;
    }

    throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
}

Cdr& Cdr::deserializeArray(long double *ldouble_t, size_t numElements, Endianness endianness)
{
    bool auxSwap = m_swapBytes;
    m_swapBytes = (m_swapBytes && (m_endianness == endianness)) || (!m_swapBytes && (m_endianness != endianness));

    try
    {
        deserializeArray(ldouble_t, numElements);
        m_swapBytes = auxSwap;
    }
    catch(Exception &ex)
    {
        m_swapBytes = auxSwap;
        ex.raise();
    }

    return *this;
}

Cdr& Cdr::serializeBoolSequence(const std::vector<bool> &vector_t)
{
    state state(*this);

    *this << (int32_t)vector_t.size();

    size_t totalSize = vector_t.size()*sizeof(bool);

    if(((m_lastPosition - m_currentPosition) >= totalSize) || resize(totalSize))
    {
        // Save last datasize.
        m_lastDataSize = sizeof(bool);

        for(size_t count = 0; count < vector_t.size(); ++count)
        {
            uint8_t value = 0;
            std::vector<bool>::const_reference ref = vector_t[count];

            if(ref)
                value = 1;
            m_currentPosition++ << value;
        }
    }
    else
    {
        setState(state);
        throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
    }

    return *this;
}

Cdr& Cdr::deserializeBoolSequence(std::vector<bool> &vector_t)
{
    uint32_t seqLength = 0;
    state state(*this);

    *this >> seqLength;

    vector_t.resize(seqLength);
    size_t totalSize = seqLength*sizeof(bool);

    if((m_lastPosition - m_currentPosition) >= totalSize)
    {
        // Save last datasize.
        m_lastDataSize = sizeof(bool);

        for(uint32_t count = 0; count < seqLength; ++count)
        {
            uint8_t value = 0;
            m_currentPosition++ >> value;

            if(value == 1)
            {
                vector_t[count] = true;
            }
            else if(value == 0)
            {
                vector_t[count] = false;
            } else {
                throw BadParamException("Unexpected byte value in Cdr::deserializeBoolSequence, expected 0 or 1");
            }
        }
    }
    else
    {
        setState(state);
        throw NotEnoughMemoryException(NotEnoughMemoryException::NOT_ENOUGH_MEMORY_MESSAGE_DEFAULT);
    }

    return *this;
}

Cdr& Cdr::deserializeStringSequence(std::string *&sequence_t, size_t &numElements)
{
    uint32_t seqLength = 0;
    state state(*this);

    deserialize(seqLength);

    try
    {
        sequence_t = (std::string*)calloc(seqLength, sizeof(std::string));
        for(uint32_t count = 0; count < seqLength; ++count)
            new(&sequence_t[count]) std::string;
        deserializeArray(sequence_t, seqLength);
    }
    catch(eprosima::fastcdr::exception::Exception &ex)
    {
        free(sequence_t);
        sequence_t = NULL;
        setState(state);
        ex.raise();
    }

    numElements = seqLength;
    return *this;
}
