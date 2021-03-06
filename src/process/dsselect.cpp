// -*-C++-*-
/*
  (c) Copyright 2003-2006, Hewlett-Packard Development Company, LP

  See the file named COPYING for license details
*/

/*
=pod

=head1 NAME

dsselect - Select subset of fields from a collection of traces, generate a new trace

=head1 SYNOPSIS

% dsselect [common-args] [--update-type] [--where='expression'] type-prefix field,field,field input.ds... output.ds

=head1 DESCRIPTION

dsselect allows you to calculate subset (both in rows and columns) of an existing dataseries file.
So you can, for example, run dsselect --where='size > 10' box_number,size boxes.ds big-boxes.ds to
extract only the box_number and size columns and only extract rows where the size is at least 10.
To get all the fields of a type simply pass 'all' instead of specific fields.

=over

=item --update-type

Allows you to modify the xml description of the extent type before registering it and writing the ds file.
This can be used to add field packing options to existing ds traces or update names, namespaces, etc.

=back

=head1 SEE ALSO

dataseries-utils(7)

=cut
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <Lintel/AssertBoost.hpp>
#include <Lintel/ProgramOptions.hpp>
#include <Lintel/StringUtil.hpp>

#include <DataSeries/commonargs.hpp>
#include <DataSeries/DataSeriesFile.hpp>
#include <DataSeries/DSExpr.hpp>
#include <DataSeries/GeneralField.hpp>
#include <DataSeries/DataSeriesModule.hpp>
#include <DataSeries/TypeIndexModule.hpp>

static bool update_type = false;
static bool get_all_fields = false;

using namespace std;
using boost::format;

lintel::ProgramOption<string> where_arg
("where", "expression controlling which lines to select, man DSExpr for details");

int main(int argc, char *argv[]) {
    Extent::setReadChecksFromEnv(true); // going to be compressing, may as well check
    commonPackingArgs packing_args;
    getPackingArgs(&argc,argv,&packing_args);

    if (string(argv[1]) == "--update-type") {
        update_type = true;
        for (int i = 2; i < argc; i++) {
            argv[i-1] = argv[i];
        }
        argc--;
    }

    lintel::programOptionsHelp("[common-args] [options] type-prefix field,field,...\n"
                               "    input-filenames... output-filename\n"
                               "common-args include:\n");
    lintel::programOptionsHelp(packingOptions());

    vector<string> extra_args = lintel::parseCommandLine(argc, argv, true);
    if (extra_args.size() < 4) {
        lintel::programOptionsUsage(argv[0]);
        exit(0);
    }

    string type_prefix(extra_args[0]);
    string fieldlist(extra_args[1]);

    vector<string> fields;
    split(fieldlist,",",fields);

    if (fieldlist == "all") {
        get_all_fields = true;
        fields.pop_back();
    }
    {
        struct stat buf;
        INVARIANT(stat(extra_args.back().c_str(), &buf) != 0, 
                  format("Refusing to overwrite existing file %s.")
                  % extra_args.back());
    }
    DataSeriesSink output(extra_args.back(), packing_args.compress_modes, 
                          packing_args.compress_level);

    // to get the complete typename and type information...
    DataSeriesSource first_file(extra_args[2]);
    const ExtentType::Ptr intype = first_file.getLibrary().getTypeByPrefixPtr(type_prefix);
    first_file.closefile();
    INVARIANT(intype != NULL, boost::format("can not find a type matching prefix %s")
              % type_prefix);

    TypeIndexModule source(intype->getName());
    for (unsigned i=2; i < (extra_args.size()-1); ++i) {
        source.addSource(extra_args[i]);
    }
    source.startPrefetching();

    ExtentSeries inputseries(ExtentSeries::typeLoose);
    ExtentSeries outputseries(ExtentSeries::typeLoose);
    vector<GeneralField *> infields, outfields;

    // TODO: figure out how to handle pack_relative options that are
    // specified relative to a field that was not selected.  Right
    // now, there is no way to do this selection; try replacing
    // enter_driver with leave_driver in
    // src/Makefile.am:ran.check-dsselect. Also, give an option for the
    // xml description to be specified on the command line.

    string xmloutdesc(str(format("<ExtentType name=\"%s\" namespace=\"%s\" version=\"%d.%d\">\n")
                          % intype->getName() % intype->getNamespace() % intype->majorVersion()
                          % intype->minorVersion()));
    inputseries.setType(intype);

    if (get_all_fields) {
        for (uint i=0; i < intype->getNFields(); i++) {
            fields.push_back( intype->getFieldName(i) );
        }
    }

    for (vector<string>::iterator i = fields.begin();
        i != fields.end();++i) {
        cout << format("%s -> %s\n") % *i % intype->xmlFieldDesc(*i);
        xmloutdesc.append(str(format("  %s\n") % intype->xmlFieldDesc(*i)));
        infields.push_back(GeneralField::create(NULL,inputseries,*i));
    }
    xmloutdesc.append("</ExtentType>\n");
    cout << xmloutdesc << "\n";

    if (update_type) {
        cout << "Update this type ? (y/n)";
        char c;
        cin >> c;
        if (c == 'y') {
            ofstream otypeinfo("temp.txt");
            otypeinfo << xmloutdesc;
            otypeinfo.close();
            cout << "Xml info has been written to temp.txt, hit enter to continue when done editing" << endl;
            cin.clear();
            cin.ignore(1,'\n');
            ifstream itypeinfo("temp.txt");                                                                                                                                                                                    
            xmloutdesc.assign( (istreambuf_iterator<char>(itypeinfo)) , (istreambuf_iterator<char>()) );                                                                                                                        
            itypeinfo.close();
            remove("temp.txt");                                                                                                                                                                                                
        }                                                                                                                                                                                                                    
    }
    
    ExtentTypeLibrary library;
    const ExtentType::Ptr outputtype(library.registerTypePtr(xmloutdesc));
    output.writeExtentLibrary(library);
    outputseries.setType(outputtype);
    for (vector<string>::iterator i = fields.begin();
        i != fields.end();++i) {
        outfields.push_back(GeneralField::create(NULL,outputseries,*i));
    }
    DSExpr *where = NULL;
    if (where_arg.used()) {
        string tmp = where_arg.get();
        where = DSExpr::make(inputseries, tmp);
    }

    OutputModule outmodule(output,outputseries,outputtype,
                           packing_args.extent_size);
    uint64_t input_row_count = 0, output_row_count = 0;
    while (true) {
        Extent::Ptr inextent = source.getSharedExtent();
        if (inextent == NULL) 
            break;
        for (inputseries.setExtent(inextent);inputseries.morerecords(); ++inputseries) {
            ++input_row_count;
            if (where && !where->valBool()) {
                continue;
            }
            ++output_row_count;
            outmodule.newRecord();
            for (unsigned int i=0;i<infields.size();++i) {
                outfields[i]->set(infields[i]);
            }
        }
    }
    outmodule.flushExtent();
    outmodule.close();
    
    GeneralField::deleteFields(infields);
    GeneralField::deleteFields(outfields);
    delete where;

    cout << format("%d input rows, %d output rows\n") % input_row_count % output_row_count;
    return 0;
}

    
