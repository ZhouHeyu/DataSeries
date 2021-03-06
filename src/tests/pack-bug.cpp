// -*-C++-*-
/*
  (c) Copyright 2010, Hewlett-Packard Development Company, LP

  See the file named COPYING for license details
*/

/** @file
    test program for DataSeries
*/

#include <DataSeries/RowAnalysisModule.hpp>
#include <DataSeries/ExtentField.hpp>
#include <DataSeries/SequenceModule.hpp>
#include <DataSeries/TypeIndexModule.hpp>
#include <DataSeries/DStoTextModule.hpp>
#include <DataSeries/commonargs.hpp>
#include <DataSeries/GeneralField.hpp>

int main(int argc, char *argv[]) {
    SINVARIANT(argc == 2);

    TypeIndexModule source("Trace::NFS::common");
    source.addSource(argv[1]);

    Extent::Ptr extent = source.getSharedExtent();
    SINVARIANT(extent != NULL);
    Extent::ByteArray packeddata;
    if (extent) {
        extent->packData(packeddata, Extent::compression_algs[Extent::compress_mode_lzf].compress_flag, 9, NULL, NULL, NULL);
        std::cout << "packed: " << packeddata.size() << " bytes\n";
    }
    source.close();
    return 0;
}
