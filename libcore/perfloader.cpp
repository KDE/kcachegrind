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
    QByteArray hostname;
    QByteArray osRelease;
    QByteArray perfVersion;
    QByteArray architecture;
    quint32 cpusOnline, cpusAvailable;
    QByteArray cpuDescription;
    QByteArray cpuId;
    quint64 memoryAvailable;
    QByteArray commandLine;
    QList<QByteArray> cpuCores;
    QList<QByteArray> cpuThreads;

    enum HeaderFlags {
        HEADER_RESERVED         = 0,    /* always cleared */
        HEADER_FIRST_FEATURE    = 1,
        HEADER_TRACING_DATA     = 1,
        HEADER_BUILD_ID,

        HEADER_HOSTNAME,
        HEADER_OSRELEASE,
        HEADER_VERSION,
        HEADER_ARCH,
        HEADER_NRCPUS,
        HEADER_CPUDESC,
        HEADER_CPUID,
        HEADER_TOTAL_MEM,
        HEADER_CMDLINE,
        HEADER_EVENT_DESC,
        HEADER_CPU_TOPOLOGY,
        HEADER_NUMA_TOPOLOGY,
        HEADER_BRANCH_STACK,
        HEADER_PMU_MAPPINGS,
        HEADER_GROUP_DESC,
        HEADER_AUXTRACE,
        HEADER_STAT,
        HEADER_CACHE,
        HEADER_LAST_FEATURE,
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
    void headerEnd();
    QByteArray readString();
    QList<QByteArray> readStringList();

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

static const QByteArray FILEMAGIC("PERFILE2");
PerfLoader::~PerfLoader()
{
}

bool PerfLoader::canLoad(QIODevice *file)
{
    Q_ASSERT(file);


    QByteArray magic = file->read(FILEMAGIC.size());
    if (magic != FILEMAGIC) {
        return false;
    }

    return true;
}

int PerfLoader::load(TraceData *, QIODevice *file, const QString &filename)
{
    Q_UNUSED(filename);

    file->reset();
    file->read(FILEMAGIC.size());

    m_stream.setDevice(file);

    quint64 headerSize, attributeSize;
    m_stream >> headerSize >> attributeSize;

    if (!readAttributes(attributeSize)) {
        return false;
    }
    if (!readData()) {
        return false;
    }
    if (!readEventTypes(attributeSize)) {
        return false;
    }

    quint64 flags;
    quint64 flags1[3];
    m_stream >> flags >> flags1[0] >> flags1[1] >> flags1[2];

    if (flags & HEADER_TRACING_DATA) {
        if (!readTracingDataHeader()) {
            return false;
        }
    }
    if (flags & HEADER_BUILD_ID) {
        if (!readBuildHeader()) {
            return false;
        }
    }

    if (flags & HEADER_HOSTNAME) {
        if (!readHostnameHeader()) {
            return false;
        }
    }

    if (flags & HEADER_OSRELEASE) {
        if (!readOsReleaseHeader()) {
            return false;
        }
    }

    if (flags & HEADER_VERSION) {
        if (!readVersionHeader()) {
            return false;
        }
    }

    if (flags & HEADER_ARCH) {
        if (!readArchHeader()) {
            return false;
        }
    }

    if (flags & HEADER_NRCPUS) {
        if (!readCpuNumberHeader()) {
            return false;
        }
    }

    if (flags & HEADER_CPUDESC) {
        if (!readCpuDescriptionHeader()) {
            return false;
        }
    }

    if (flags & HEADER_CPUID) {
        if (!readCpuIdHeader()) {
            return false;
        }
    }

    if (flags & HEADER_TOTAL_MEM) {
        if (!readTotalMemHeader()) {
            return false;
        }
    }

    if (flags & HEADER_CMDLINE) {
        if (!readCmdLineHeader()) {
            return false;
        }
    }

    if (flags & HEADER_EVENT_DESC) {
        if (!readEventDescHeader()) {
            return false;
        }
    }

    if (flags & HEADER_CPU_TOPOLOGY) {
        if (!readCpuTopologyHeader()) {
            return false;
        }
    }

    if (flags & HEADER_NUMA_TOPOLOGY) {
        if (!readNumaTopologyHeader()) {
            return false;
        }
    }

    if (flags & HEADER_BRANCH_STACK) {
        if (!readBranchStackHeader()) {
            return false;
        }
    }

    if (flags & HEADER_PMU_MAPPINGS) {
        if (!readPmuMappingHeader()) {
            return false;
        }
    }

    if (flags & HEADER_GROUP_DESC) {
        if (!readGroupDescHeader()) {
            return false;
        }
    }

    if (flags & HEADER_AUXTRACE) {
        if (!readAuxTraceHeader()) {
            return false;
        }
    }

    if (flags & HEADER_STAT) {
        if (!readStatHeader()) {
            return false;
        }
    }

    if (flags & HEADER_CACHE) {
        if (!readCacheHeader()) {
            return false;
        }
    }

    return true;

}

bool PerfLoader::readAttributes(quint64 attributeSize)
{
    quint64 length = headerStart();

    qDebug() << length / attributeSize;

    /*
     * This is an array of perf_event_attrs, each attr_size bytes long, which defines
     * each event collected. See perf_event.h or the man page for a detailed
     * description.
     */

    headerEnd();

    return true;
}

bool PerfLoader::readData()
{
    headerStart();

    /*
     * This section is the bulk of the file. It consist of a stream of perf_events
     * describing events. This matches the format generated by the kernel.
     * See perf_event.h or the manpage for a detailed description.
     *
     * see: https://lwn.net/Articles/644919/
     */

    headerEnd();

    return true;
}

bool PerfLoader::readEventTypes(quint64 attributeSize)
{
    headerStart();


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
        quint64 length = headerStart();
        quint64 ids[length];
        for (size_t i=0; i<length / sizeof(quint64); i++) {
            m_stream >> ids[i];
        }

        headerEnd();
    }

//    }

    headerEnd();

    return true;
}

quint64 PerfLoader::headerStart()
{
    quint64 offset, length;
    m_stream >> offset >> length;

    m_stream.device()->startTransaction();
    m_stream.device()->seek(offset);

    return length;
}

void PerfLoader::headerEnd()
{
    m_stream.device()->rollbackTransaction();
}

QByteArray PerfLoader::readString()
{
    quint32 stringLength;
    m_stream >> stringLength;
    return m_stream.device()->read(stringLength);
}

QList<QByteArray> PerfLoader::readStringList()
{
    QList<QByteArray> list;

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
    headerEnd();

    return true;
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

    headerEnd();

    return true;
}

bool PerfLoader::readHostnameHeader()
{
    headerStart();
    hostname = readString();
    headerEnd();

    return true;
}

bool PerfLoader::readOsReleaseHeader()
{
    headerStart();
    osRelease = readString();
    headerEnd();

    return true;
}

bool PerfLoader::readVersionHeader()
{
    headerStart();
    perfVersion = readString();
    headerEnd();

    return true;
}

bool PerfLoader::readArchHeader()
{
    headerStart();
    architecture = readString();
    headerEnd();

    return true;
}

bool PerfLoader::readCpuNumberHeader()
{
    headerStart();
    m_stream >> cpusOnline >> cpusAvailable;
    headerEnd();

    return true;
}

bool PerfLoader::readCpuDescriptionHeader()
{
    headerStart();
    cpuDescription = readString();
    headerEnd();

    return true;
}

bool PerfLoader::readCpuIdHeader()
{
    headerStart();
    cpuId = readString();
    headerEnd();

    return true;
}

bool PerfLoader::readTotalMemHeader()
{
    headerStart();
    m_stream >> memoryAvailable;
    headerEnd();

    return true;
}

bool PerfLoader::readCmdLineHeader()
{
    headerStart();
    commandLine = readString();
    headerEnd();

    return true;
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

    headerEnd();

    return true;
}

bool PerfLoader::readCpuTopologyHeader()
{
    headerStart();
    cpuCores = readStringList();
    cpuThreads = readStringList();
    headerEnd();

    return true;
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

   headerEnd();

   return true;
}

bool PerfLoader::readBranchStackHeader()
{
    headerStart();

    // Not implemented in perf.

    headerEnd();
    return true;
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

    headerEnd();

    return true;
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


    headerEnd();

    return true;
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

    headerEnd();
    return true;
}

bool PerfLoader::readStatHeader()
{
    headerStart();
    // TODO
    headerEnd();
    return true;
}

bool PerfLoader::readCacheHeader()
{
    headerStart();
    //TODO
    headerEnd();
    return true;
}

