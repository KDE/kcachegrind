#include "loader.h"
#include <QIODevice>
#include <QDataStream>
#include <QDebug>

struct perf_file_section {
    uint64_t offset;	/* offset from start of file */
    uint64_t size;		/* size of the section */
};

struct perf_header {
    char magic[8];    	/* PERFILE2 */
    uint64_t size;    	/* size of the header */
    uint64_t attr_size;   	/* size of an attribute in attrs */
    struct perf_file_section attrs;
    struct perf_file_section data;
    struct perf_file_section event_types;
    uint64_t flags;
    uint64_t flags1[3];
};

class PerfLoader : public Loader
{
public:
    QString hostname;
    QString osRelease;
    QString perfVersion;
    QString architecture;
    quint32 cpusOnline, cpusAvailable;
    QString cpuDescription;
    QString cpuId;
    quint64 memoryAvailable;
    QString commandLine;
    QList<QString> cpuCores;
    QList<QString> cpuThreads;

    enum HeaderFlags {
        HEADER_RESERVED         = 0,    /* always cleared */
        HEADER_FIRST_FEATURE    = 1,
        HEADER_TRACING_DATA     = 1,
        HEADER_BUILD_ID = (1 << 2),

        HEADER_HOSTNAME = (1 << 3),
        HEADER_OSRELEASE = (1 << 4),
        HEADER_VERSION = (1 << 5),
        HEADER_ARCH = (1 << 6),
        HEADER_NRCPUS = (1 << 7),
        HEADER_CPUDESC = (1 << 8),
        HEADER_CPUID = (1 << 9),
        HEADER_TOTAL_MEM = (1 << 10),
        HEADER_CMDLINE = (1 << 11),
        HEADER_EVENT_DESC = (1 << 12),
        HEADER_CPU_TOPOLOGY = (1 << 13),
        HEADER_NUMA_TOPOLOGY = (1 << 14),
        HEADER_BRANCH_STACK = (1 << 15),
        HEADER_PMU_MAPPINGS = (1 << 16),
        HEADER_GROUP_DESC = (1 << 17),
        HEADER_AUXTRACE = (1 << 18),
        HEADER_STAT = (1 << 19),
        HEADER_CACHE = (1 << 20),
        HEADER_LAST_FEATURE = (1 << 21),
        HEADER_FEAT_BITS        = 256,
     };


    PerfLoader();
    ~PerfLoader();
    bool canLoad(QIODevice *file) override;
    int load(TraceData *, QIODevice *file, const QString &filename) override;

    bool readAttributes(quint64 attributeSize);
    bool readData();
    bool readEventTypes(quint64 attributeSize);

    quint64 headerStart();
    bool headerEnd();
    QString readString();
    QStringList readStringList();

    bool readTracingDataHeader();
    bool readBuildHeader();
    bool readHostnameHeader();
    bool readOsReleaseHeader();
    bool readVersionHeader();
    bool readArchHeader();
    bool readCpuNumberHeader();
    bool readCpuDescriptionHeader();
    bool readCpuIdHeader();
    bool readTotalMemHeader();
    bool readCmdLineHeader();
    bool readEventDescHeader();
    bool readCpuTopologyHeader();
    bool readNumaTopologyHeader();
    bool readBranchStackHeader();
    bool readPmuMappingHeader();
    bool readGroupDescHeader();
    bool readAuxTraceHeader();
    bool readStatHeader();
    bool readCacheHeader();

    quint64 dataOffset;
    quint64 dataSize;

    QDataStream m_stream;
};

Loader* createPerfLoader()
{
  return new PerfLoader();
}

PerfLoader::PerfLoader() :
    Loader(QStringLiteral("Perf"), QObject::tr("Import filter for perf.data"))
{
}

PerfLoader::~PerfLoader()
{
}

static const QByteArray FILEMAGIC("PERFILE2");
static const quint64 MAGIC_BIG_ENDIAN = 0x32454c4946524550ULL;
static const quint64 MAGIC_LITTLE_ENDIAN = 0x50455246494c4532ULL;

bool PerfLoader::canLoad(QIODevice *file)
{
    Q_ASSERT(file);

    file->reset();

    QDataStream stream(file);
    quint64 magic;
    stream >> magic;
    if (magic != MAGIC_LITTLE_ENDIAN && magic != MAGIC_BIG_ENDIAN) {
        qDebug() << "Invalid magic:" << magic;
        return false;
    }

    return true;
}

int PerfLoader::load(TraceData *, QIODevice *file, const QString &filename)
{
    Q_UNUSED(filename);

    file->reset();
    m_stream.setDevice(file);

    quint64 magic;
    m_stream >> magic;
    if (magic == MAGIC_LITTLE_ENDIAN) {
        m_stream.setByteOrder(QDataStream::LittleEndian);
    } else {
        m_stream.setByteOrder(QDataStream::BigEndian);
    }

    quint64 headerSize, attributeSize;
    m_stream >> headerSize >> attributeSize;
    qDebug() << "Header size:" << headerSize;
    qDebug() << "Attribute size:" << attributeSize;

    if (!readAttributes(attributeSize)) {
        qWarning() << "Can't read attributes";
        return false;
    }
    if (!readData()) {
        qWarning() << "Can't read data";
        return false;
    }
    if (!readEventTypes(attributeSize)) {
        qWarning() << "Can't read event types";
        return false;
    }

    quint64 flags;
    m_stream >> flags;

    m_stream.device()->seek(dataOffset + dataSize);

    if (flags & HEADER_TRACING_DATA) {
        if (!readTracingDataHeader()) {
            return false;
        }
    }
    if (flags & HEADER_BUILD_ID) {
        if (!readBuildHeader()) {
            qWarning() << "Invalid build header";
            return false;
        }
    }

    if (flags & HEADER_HOSTNAME) {
        if (!readHostnameHeader()) {
            qWarning() << "Invalid hostname header";
            return false;
        }
    } else {
        qWarning() << "Hostname feature is guaranteed to be here, something is wrong";
        return false;
    }

    if (flags & HEADER_OSRELEASE) {
        if (!readOsReleaseHeader()) {
            qWarning() << "Invalid OS release header";
            return false;
        }
    }

    if (flags & HEADER_VERSION) {
        if (!readVersionHeader()) {
            qWarning() << "Invalid version header";
            return false;
        }
    }

    if (flags & HEADER_ARCH) {
        if (!readArchHeader()) {
            qWarning() << "Invalid architecture header";
            return false;
        }
    }

    if (flags & HEADER_NRCPUS) {
        if (!readCpuNumberHeader()) {
            qWarning() << "Invalid cpu number header";
            return false;
        }
    }

    if (flags & HEADER_CPUDESC) {
        if (!readCpuDescriptionHeader()) {
            qWarning() << "Invalid cpu description header";
            return false;
        }
    }

    if (flags & HEADER_CPUID) {
        if (!readCpuIdHeader()) {
            qWarning() << "Invalid cpu id header";
            return false;
        }
    }

    if (flags & HEADER_TOTAL_MEM) {
        if (!readTotalMemHeader()) {
            qWarning() << "Invalid total memory header";
            return false;
        }
    }

    if (flags & HEADER_CMDLINE) {
        if (!readCmdLineHeader()) {
            qWarning() << "Invalid command line header";
            return false;
        }
    }

    if (flags & HEADER_EVENT_DESC) {
        if (!readEventDescHeader()) {
            qWarning() << "Invalid command description header";
            return false;
        }
    }

    if (flags & HEADER_CPU_TOPOLOGY) {
        if (!readCpuTopologyHeader()) {
            qWarning() << "Invalid cpu topology header";
            return false;
        }
    }

    if (flags & HEADER_NUMA_TOPOLOGY) {
        if (!readNumaTopologyHeader()) {
            qWarning() << "Invalid numa topology header";
            return false;
        }
    }

    if (flags & HEADER_BRANCH_STACK) {
        if (!readBranchStackHeader()) {
            qWarning() << "Invalid branch stack header";
            return false;
        }
    }

    if (flags & HEADER_PMU_MAPPINGS) {
        if (!readPmuMappingHeader()) {
            qWarning() << "Invalid pmu mapping header";
            return false;
        }
    }

    if (flags & HEADER_GROUP_DESC) {
        if (!readGroupDescHeader()) {
            qWarning() << "Invalid group description header";
            return false;
        }
    }

    if (flags & HEADER_AUXTRACE) {
        if (!readAuxTraceHeader()) {
            qWarning() << "Invalid aux trace header";
            return false;
        }
    }

    if (flags & HEADER_STAT) {
        if (!readStatHeader()) {
            qWarning() << "Invalid stat header";
            return false;
        }
    }

    if (flags & HEADER_CACHE) {
        if (!readCacheHeader()) {
            qWarning() << "Invalid cache header";
            return false;
        }
    }

    return true;

}

bool PerfLoader::readAttributes(quint64 attributeSize)
{
    Q_UNUSED(attributeSize);

    qDebug() << "Attribute section size:" << headerStart();


    /*
     * This is an array of perf_event_attrs, each attr_size bytes long, which defines
     * each event collected. See perf_event.h or the man page for a detailed
     * description.
     */

    return headerEnd();
}

bool PerfLoader::readData()
{
    dataSize = headerStart();
    dataOffset = m_stream.device()->pos();

    qDebug() << "Data size" << dataSize;

    /*
     * This section is the bulk of the file. It consist of a stream of perf_events
     * describing events. This matches the format generated by the kernel.
     * See perf_event.h or the manpage for a detailed description.
     *
     * see: https://lwn.net/Articles/644919/
     */

    return headerEnd();
}

bool PerfLoader::readEventTypes(quint64 attributeSize)
{
    qDebug() << "Event types length" << headerStart();


    /*
     * Define the event attributes with their IDs.
     * An array bound by the perf_file_section size.
     */

//    struct {
//        struct perf_event_attr attr;   /* Size defined by header.attr_size */
    m_stream.skipRawData(attributeSize);

//        struct perf_file_section ids;
//      ids points to a array of uint64_t defining the ids for event attr attr.
    {
//        quint64 length = headerStart();
//        quint64 ids[length];
//        for (size_t i=0; i<length / sizeof(quint64); i++) {
//            m_stream >> ids[i];
//        }

//        headerEnd();
    }

//    }

    return headerEnd();
}

quint64 PerfLoader::headerStart()
{
    qint64 offset, length;
    m_stream >> offset >> length;
    if (offset > m_stream.device()->size()) {
        qWarning() << "Invalid header offset" << offset;
        m_stream.device()->close();
        return 0;
    }

    m_stream.device()->startTransaction();
    m_stream.device()->seek(offset);

    return length;
}

bool PerfLoader::headerEnd()
{
    QString errorString = m_stream.device()->errorString();
    if (!m_stream.device()->isTransactionStarted()) {
        qWarning() << "Transaction not started!";
        return false;
    }
    if (m_stream.status() != QDataStream::Ok) {
        qWarning() << "Error while ending transactions" << errorString;
        m_stream.device()->rollbackTransaction();
        return false;
    }
    m_stream.device()->rollbackTransaction();

    return true;
}

QString PerfLoader::readString()
{
    quint32 stringLength;
    m_stream >> stringLength;
    if (stringLength > 512) {
        qWarning() << "Abnormal string length";
        m_stream.device()->close();
        return QString();
    }
    return QString::fromLocal8Bit(m_stream.device()->read(stringLength));
}

QStringList PerfLoader::readStringList()
{
    QStringList list;

    quint32 stringsCount;
    m_stream >> stringsCount;
    for (quint32 i = 0; i < stringsCount; i++) {
        list.append(readString());
    }

    return list;
}

bool PerfLoader::readTracingDataHeader()
{
    headerStart();
    //TODO
    return headerEnd();
}

bool PerfLoader::readBuildHeader()
{
    headerStart();
    /*
     * The header consists of an sequence of build_id_event. The size of each record
     * is defined by header.size (see perf_event.h). Each event defines a ELF build id
     * for a executable file name for a pid. An ELF build id is a unique identifier
     * assigned by the linker to an executable.
     *
     */

//    struct build_id_event {
//        struct perf_event_header header;
//        pid_t			 pid;
//        uint8_t			 build_id[24];
//        char			 filename[header.size - offsetof(struct build_id_event, filename)];
//    };

    return headerEnd();
}

bool PerfLoader::readHostnameHeader()
{
    headerStart();
    hostname = readString();
    qDebug() << "Hostname:" << hostname;
    return headerEnd();
}

bool PerfLoader::readOsReleaseHeader()
{
    headerStart();
    osRelease = readString();
    qDebug() << "OS release:" << osRelease;
    return headerEnd();
}

bool PerfLoader::readVersionHeader()
{
    headerStart();
    perfVersion = readString();
    qDebug() << "perf version:" << perfVersion;
    return headerEnd();
}

bool PerfLoader::readArchHeader()
{
    headerStart();
    architecture = readString();
    qDebug() << "Architecture" << architecture;
    return headerEnd();
}

bool PerfLoader::readCpuNumberHeader()
{
    headerStart();
    m_stream >> cpusOnline >> cpusAvailable;
    qDebug() << "CPUs online" << cpusOnline;
    qDebug() << "CPUs available" << cpusAvailable;
    return headerEnd();
}

bool PerfLoader::readCpuDescriptionHeader()
{
    headerStart();
    cpuDescription = readString();
    qDebug() << "CPU description:" << cpuDescription;
    return headerEnd();
}

bool PerfLoader::readCpuIdHeader()
{
    headerStart();
    cpuId = readString();
    qDebug() << "CPU ID:" << cpuId;
    return headerEnd();
}

bool PerfLoader::readTotalMemHeader()
{
    headerStart();
    m_stream >> memoryAvailable;
    qDebug() << "Total memory available:" << memoryAvailable;
    return headerEnd();
}

bool PerfLoader::readCmdLineHeader()
{
    headerStart();
    commandLine = readString();
    qDebug() << "Command line:" << commandLine;
    return headerEnd();
}

bool PerfLoader::readEventDescHeader()
{
    headerStart();

    /*
     * Another description of the perf_event_attrs, more detailed than header.attrs
     * including IDs and names. See perf_event.h or the man page for a description
     * of a struct perf_event_attr.
     */

//    struct {
//        uint32_t nr; // number of events
//        uint32_t attr_size; // size of each perf_event_attr
//        struct {
//            struct perf_event_attr attr;  // size of attr_size
//            uint32_t nr_ids;
//            struct perf_header_string event_string;
//            uint64_t ids[nr_ids];
//        } events[nr]; // Variable length records
//    };

    return headerEnd();
}

bool PerfLoader::readCpuTopologyHeader()
{
    headerStart();
    cpuCores = readStringList();
    cpuThreads = readStringList();

    qDebug() << "CPU cores:" << cpuCores;
    qDebug() << "CPU threads:" << cpuThreads;
    return headerEnd();
}

bool PerfLoader::readNumaTopologyHeader()
{
   headerStart();
   /*
    *
    * A list of NUMA node descriptions
    *
    */

//   struct {
//       uint32_t nr;
//       struct {
//           uint32_t nodenr;
//           uint64_t mem_total;
//           uint64_t mem_free;
//           struct perf_header_string cpus;
//       } nodes[nr]; /* Variable length records */
//   };

   return headerEnd();
}

bool PerfLoader::readBranchStackHeader()
{
    headerStart();

    // Not implemented in perf.

    return headerEnd();
}

bool PerfLoader::readPmuMappingHeader()
{
    headerStart();

    /*
     * A list of PMU structures, defining the different PMUs supported by perf.
     */

//    struct {
//           uint32_t nr;
//           struct pmu {
//                  uint32_t pmu_type;
//              struct perf_header_string pmu_name;
//           } [nr]; /* Variable length records */
//    };

    return headerEnd();
}

bool PerfLoader::readGroupDescHeader()
{
    headerStart();

    // Description of counter groups ({...} in perf syntax)

//    struct {
//             uint32_t nr;
//             struct {
//                struct perf_header_string string;
//            uint32_t leader_idx;
//            uint32_t nr_members;
//         } [nr]; /* Variable length records */
//    };


    return headerEnd();
}

bool PerfLoader::readAuxTraceHeader()
{
    headerStart();

    /*
     * Define additional auxtrace areas in the perf.data. auxtrace is used to store
     * undecoded hardware tracing information, such as Intel Processor Trace data.
     */


    /**
     * struct auxtrace_index_entry - indexes a AUX area tracing event within a
     *                               perf.data file.
     * @file_offset: offset within the perf.data file
     * @sz: size of the event
     */
//    struct auxtrace_index_entry {
//        u64			file_offset;
//        u64			sz;
//    };

//    #define PERF_AUXTRACE_INDEX_ENTRY_COUNT 256

    /**
     * struct auxtrace_index - index of AUX area tracing events within a perf.data
     *                         file.
     * @list: linking a number of arrays of entries
     * @nr: number of entries
     * @entries: array of entries
     */
//    struct auxtrace_index {
//        struct list_head	list;
//        size_t			nr;
//        struct auxtrace_index_entry entries[PERF_AUXTRACE_INDEX_ENTRY_COUNT];
//    };

//    other bits are reserved and should ignored for now

    return headerEnd();
}

bool PerfLoader::readStatHeader()
{
    headerStart();
    // TODO
    return headerEnd();
}

bool PerfLoader::readCacheHeader()
{
    headerStart();
    //TODO
    return headerEnd();
}

