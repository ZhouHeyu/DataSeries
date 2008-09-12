#include <Lintel/HashMap.hpp>

#include <DataSeries/RowAnalysisModule.hpp>
#include <DataSeries/SequenceModule.hpp>
#include <DataSeries/TypeIndexModule.hpp>

using namespace std;
using boost::format;

// TODO: figure out why we can run with fieldnames empty (missing call
// to firstExtent)

class MissingOps : public RowAnalysisModule {
public:
    MissingOps(DataSeriesModule &source)
        : RowAnalysisModule(source),
	  packet_at(series, ""),
          source(series, "source"),
	  dest(series, "dest"),
          is_request(series, ""),
          transaction_id(series, ""),
	  payload_length(series, ""),
          record_id(series, ""),
	  good_count(0), skip_count(0), skip_rec_count(0), backwards_count(0), request_count(0),
	  old_count(0),
	  age_out_raw(0), last_report_count(0)
    { }

    virtual ~MissingOps() { }

    virtual void firstExtent(const Extent &e) {
	const ExtentType &type = e.getType();
	if (type.versionCompatible(0,0) || type.versionCompatible(1,0)) {
	    packet_at.setFieldName("packet-at");
	    is_request.setFieldName("is-request");
	    transaction_id.setFieldName("transaction-id");
	    payload_length.setFieldName("payload-length");
	    record_id.setFieldName("record-id");
	} else if (type.versionCompatible(2,0)) {
	    packet_at.setFieldName("packet_at");
	    is_request.setFieldName("is_request");
	    transaction_id.setFieldName("transaction_id");
	    payload_length.setFieldName("payload_length");
	    record_id.setFieldName("record_id");
	} else {
	    FATAL_ERROR(format("can only handle v[0,1,2].*; not %d.%d")
			% type.majorVersion() % type.minorVersion());
	}
    }

    virtual void newExtentHook (const Extent &e) {
	//	cout << format("Hi %d\n") % request_count;
	if (request_count > last_report_count + 10000000) {
	    report(0);
	    last_report_count = request_count;
	}
    }

    enum FlipMode { Unknown, Flip, NoFlip };
    struct XactInfo {
	uint32_t xact_id;
	int64_t at;
	XactInfo() : xact_id(0), at(0) { }
	XactInfo(uint32_t a, int64_t b) : xact_id(a), at(b) { }
	bool operator <(const XactInfo &rhs) const {
	    return xact_id < rhs.xact_id;
	}
    };

    struct Info {
	vector<XactInfo> last_xact_ids;
	FlipMode flipmode;
	Info() : flipmode(Unknown) { }
    };

    void flipVec(vector<XactInfo> &vec) {
	for(vector<XactInfo>::iterator i = vec.begin(); i != vec.end(); ++i) {
	    i->xact_id = Extent::flip4bytes(i->xact_id);
	}
    }

    uint32_t countOneSkip(vector<XactInfo> &vec) {
	uint32_t ret = 0;
	uint32_t prev = vec[0].xact_id;
	for(vector<XactInfo>::iterator i = vec.begin() + 1; i != vec.end(); ++i) {
	    if (i->xact_id == (prev + 1)) {
		++ret;
	    } 
	    prev = i->xact_id;
	}
	return ret;
    }

    void dumpVec(vector<XactInfo> &vec) {
	for(vector<XactInfo>::iterator i = vec.begin(); i != vec.end(); ++i) {
	    cout << format("%08x, ") % i->xact_id;
	}
    }

    void determineFlipMode(int32_t client_id, Info &info) {
	static const double required_skip1 = 0.5;
	vector<XactInfo> tmp = info.last_xact_ids;

	sort(tmp.begin(), tmp.end());
	uint32_t skip1_count = countOneSkip(tmp);
	if (skip1_count > tmp.size() * required_skip1) {
	    info.flipmode = NoFlip;
	    return;
	} 
	tmp = info.last_xact_ids;
	flipVec(tmp);
	sort(tmp.begin(), tmp.end());
	uint32_t skipf_count = countOneSkip(tmp);
	if (skipf_count > tmp.size() * required_skip1) {
	    if (false) {
		cout << format("flip %d > %d\n") % skipf_count % (tmp.size() * required_skip1);
		dumpVec(tmp);
		cout << "\n";
	    }
	    info.last_xact_ids = tmp;
	    info.flipmode = Flip;
	    return;
	} 
	cout << format("Badness guessing on %08x: %d,%d vs %d\n") % client_id
	    % skip1_count % skipf_count % (tmp.size() * required_skip1);
	dumpVec(info.last_xact_ids);
	cout << "\n";
    }

    void processBatch(int32_t client_id, Info &info, int64_t now) {
	if (age_out_raw == 0) {
	    age_out_raw = packet_at.secNanoToRaw(1,0);
	}
	if (false) cout << format("PB %x %d %d; %d\n") % client_id % request_count % good_count % info.last_xact_ids.size();
	if (info.flipmode == Unknown) {
	    determineFlipMode(client_id, info);
	    if (info.flipmode == Unknown) {
		return;
	    }
	    if (false) cout << format("%x select %d\n") % client_id % info.flipmode;
	} 
	sort(info.last_xact_ids.begin(), info.last_xact_ids.end());
	
	vector<XactInfo> new_vec;
	new_vec.reserve(info.last_xact_ids.size());
	SINVARIANT(!info.last_xact_ids.empty());
	vector<XactInfo>::iterator i = info.last_xact_ids.begin();
	while(true) {
	    vector<XactInfo>::iterator nexti = i + 1;
	    if (nexti == info.last_xact_ids.end()) {
		break;
	    }
	    uint32_t skip = nexti->xact_id - i->xact_id - 1;
	    if (skip == 0) {
		++good_count;
	    } else if (skip > static_cast<uint32_t>(4294967295LL - 1000)) {
		++backwards_count;
	    } else if (skip < 128) { // 7 bits
		if (false) cout << format("skip %x: %d\n") % client_id % skip;
		skip_count += skip;
		++skip_rec_count;
	    } else if (i->at < now - age_out_raw) {
		++old_count;
	    } else {
		new_vec.push_back(*i);
	    }
	    i = nexti;
	}
	if (info.last_xact_ids.back().at < now - age_out_raw) {
	    ++old_count;
	} else {
	    new_vec.push_back(info.last_xact_ids.back());
	}
	if (new_vec.size() > info.last_xact_ids.size() / 2 && new_vec.size() > 100) {
	    cout << format("Badness on %08x(%d) %d vs %d: ") 
		% client_id % info.flipmode % new_vec.size() % info.last_xact_ids.size();
	    dumpVec(new_vec);
	    FATAL_ERROR("no");
	}

	info.last_xact_ids.swap(new_vec);
	//	cout << format("PB %x %d %d; %d\n") % client_id % request_count % good_count % info.last_xact_ids.size();
    }


    virtual void processRow() {
	if (!is_request.val()) {
	    return;
	}
	++request_count;

	Info &info = client_to_info[source.val()];
	switch(info.flipmode) 
	    {
	    case Unknown: 
	    case NoFlip:
		info.last_xact_ids.push_back(XactInfo(static_cast<uint32_t>(transaction_id.val()),
						      packet_at.valRaw()));
		break;
	    case Flip:
		info.last_xact_ids.push_back(XactInfo(Extent::flip4bytes(transaction_id.val()),
						      packet_at.valRaw()));
		break;
	    default:
		FATAL_ERROR("?");
	    }

	if (info.last_xact_ids.size() >= 1000) {
	    processBatch(source.val(), info, packet_at.valRaw());
	}
    }

    void report(uint64_t unknown_count) {
	cout << format("request %d; good %d %.2f%%; skip %d (%.4f%%); backwards %d; old %d; unknown %d\n") 
	    % request_count % good_count % (100.0*good_count / request_count)
	    % skip_count % (100.0*skip_count / (good_count + skip_count))
	    % backwards_count % old_count % unknown_count;
    }

    virtual void printResult() {
	uint64_t unknown_count = 0;
	for(HashMap<int32_t, Info>::iterator i = client_to_info.begin();
	    i != client_to_info.end(); ++i) {
	    if (i->second.flipmode != Unknown) {
		processBatch(i->first, i->second, age_out_raw);
	    }
	    unknown_count += i->second.last_xact_ids.size();
	}
	report(unknown_count);
    }

    HashMap<int32_t, Info> client_to_info;

private:
    Int64TimeField packet_at;
    Int32Field source;
    Int32Field dest;
    //    BoolField is_udp;
    BoolField is_request;
    //    ByteField nfs_version;
    Int32Field transaction_id;
    Int32Field payload_length;
    Int64Field record_id;
    uint64_t good_count, skip_count, skip_rec_count, backwards_count, request_count, old_count;
    int64_t age_out_raw;
    uint64_t last_report_count;
};

namespace NFSDSAnalysisMod {
    RowAnalysisModule *newMissingOps(DataSeriesModule &prev) {
	return new MissingOps(prev);
    }
}
