// Regression test for the Common Instance Reference module of created
// segmentations (https://github.com/QIICR/dcmqi/issues/554).
//
// Builds two constellations that the reference bookkeeping used to get wrong,
// by deriving additional source instances from the given classic series in
// memory (no fixture files needed):
//
//  1. A second instance with the same geometry as an existing one, so that two
//     source instances match the same segmentation frame. Both must appear in
//     the frame's Source Image Sequence AND in the top-level Referenced
//     Instance Sequence; previously only the first one per frame reached the
//     Common Instance Reference module.
//  2. An instance from a second series of the same study. The Referenced
//     Series Sequence must group instances by the series they actually belong
//     to; previously all instances were attributed to the series of the first
//     input dataset.
//
// Cross-study references (Studies Containing Other Referenced Instances
// Sequence) are out of scope here and not supported yet.

#include "dcmqi/Itk2DicomConverter.h"

#include <dcmtk/dcmdata/dcdatset.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcsequen.h>
#include <dcmtk/dcmdata/dcuid.h>

#include <itkImageFileReader.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{
using ImageType = itk::Image<short, 3U>;
using ReaderType = itk::ImageFileReader<ImageType>;

#define REQUIRE(expr)                                                                  \
  do {                                                                                 \
    if (!(expr)) {                                                                     \
      std::cerr << "FAIL: " << #expr << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
      return EXIT_FAILURE;                                                             \
    }                                                                                  \
  } while (0)

std::string readFile(const std::string& path)
{
  std::ifstream in(path.c_str(), std::ios::binary);
  if (!in)
  {
    std::cerr << "ERROR: cannot open " << path << std::endl;
    return std::string();
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

DcmDataset* loadDataset(const std::string& path)
{
  DcmFileFormat ff;
  if (ff.loadFile(path.c_str()).bad())
  {
    std::cerr << "ERROR: cannot read " << path << std::endl;
    return nullptr;
  }
  return OFstatic_cast(DcmDataset*, ff.getDataset()->clone());
}

// Copy of the given dataset with a fresh SOP Instance UID and, if requested,
// a fresh Series Instance UID; geometry and everything else stay identical.
DcmDataset* deriveInstance(DcmDataset& original, OFString& sopInstanceUID,
                           const bool newSeries, OFString& seriesInstanceUID)
{
  std::unique_ptr<DcmDataset> copy(OFstatic_cast(DcmDataset*, original.clone()));
  char uid[100];
  if (copy->putAndInsertString(DCM_SOPInstanceUID,
                               dcmGenerateUniqueIdentifier(uid, SITE_INSTANCE_UID_ROOT)).bad())
    return nullptr;
  sopInstanceUID = uid;
  if (newSeries)
  {
    if (copy->putAndInsertString(DCM_SeriesInstanceUID,
                                 dcmGenerateUniqueIdentifier(uid, SITE_SERIES_UID_ROOT)).bad())
      return nullptr;
  }
  copy->findAndGetOFString(DCM_SeriesInstanceUID, seriesInstanceUID);
  return copy.release();
}

// Per frame of the segmentation, the SOP Instance UIDs its Derivation Image
// FG references
std::vector<std::set<OFString> > frameReferences(DcmDataset& seg)
{
  std::vector<std::set<OFString> > result;
  DcmSequenceOfItems* perFrameSeq = nullptr;
  if (seg.findAndGetSequence(DCM_PerFrameFunctionalGroupsSequence, perFrameSeq).bad() || !perFrameSeq)
    return result;
  for (unsigned long f = 0; f < perFrameSeq->card(); ++f)
  {
    std::set<OFString> refs;
    DcmItem* derivationItem = nullptr;
    if (perFrameSeq->getItem(f)->findAndGetSequenceItem(DCM_DerivationImageSequence, derivationItem, 0).good()
        && derivationItem)
    {
      DcmSequenceOfItems* sourceImageSeq = nullptr;
      if (derivationItem->findAndGetSequence(DCM_SourceImageSequence, sourceImageSeq).good() && sourceImageSeq)
      {
        for (unsigned long s = 0; s < sourceImageSeq->card(); ++s)
        {
          OFString uid;
          if (sourceImageSeq->getItem(s)->findAndGetOFString(DCM_ReferencedSOPInstanceUID, uid).good())
            refs.insert(uid);
        }
      }
    }
    result.push_back(refs);
  }
  return result;
}
}

int main(int argc, char* argv[])
{
  if (argc != 6)
  {
    std::cerr << "Usage: " << argv[0]
              << " <metadata.json> <segmentation.nrrd> <dicom1> <dicom2> <dicom3>" << std::endl;
    return EXIT_FAILURE;
  }

  const std::string metadata = readFile(argv[1]);
  REQUIRE(!metadata.empty());

  ReaderType::Pointer reader = ReaderType::New();
  reader->SetFileName(argv[2]);
  try
  {
    reader->Update();
  }
  catch (const itk::ExceptionObject& e)
  {
    std::cerr << "ERROR: failed to load segmentation: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  std::vector<ImageType::ConstPointer> segmentations;
  segmentations.emplace_back(reader->GetOutput());

  // the classic series (one study, one series)
  std::vector<DcmItem*> datasets;
  std::vector<OFString> uids;
  OFString firstSeriesUID;
  for (int i = 3; i < 6; ++i)
  {
    DcmDataset* ds = loadDataset(argv[i]);
    REQUIRE(ds != nullptr);
    OFString uid;
    REQUIRE(ds->findAndGetOFString(DCM_SOPInstanceUID, uid).good());
    if (i == 3)
      REQUIRE(ds->findAndGetOFString(DCM_SeriesInstanceUID, firstSeriesUID).good());
    datasets.push_back(ds);
    uids.push_back(uid);
  }

  // a second instance on the plane of the first one, same series (issue #554)
  OFString duplicateUID, duplicateSeriesUID;
  DcmDataset* duplicate = deriveInstance(*OFstatic_cast(DcmDataset*, datasets[0]),
                                         duplicateUID, false, duplicateSeriesUID);
  REQUIRE(duplicate != nullptr);
  REQUIRE(duplicateSeriesUID == firstSeriesUID);
  datasets.push_back(duplicate);

  // an instance on the plane of the second one, from a second series
  OFString otherSeriesInstanceUID, otherSeriesUID;
  DcmDataset* otherSeries = deriveInstance(*OFstatic_cast(DcmDataset*, datasets[1]),
                                           otherSeriesInstanceUID, true, otherSeriesUID);
  REQUIRE(otherSeries != nullptr);
  REQUIRE(otherSeriesUID != firstSeriesUID);
  datasets.push_back(otherSeries);

  std::unique_ptr<DcmDataset> seg(
      dcmqi::Itk2DicomConverter::itkimage2dcmSegmentation(datasets, segmentations, metadata,
                                                          false /* skipEmptySlices */));
  REQUIRE(seg != nullptr);

  // Frame level: instances sharing a plane must be referenced together
  const std::vector<std::set<OFString> > perFrame = frameReferences(*seg);
  REQUIRE(!perFrame.empty());
  std::set<OFString> frameUnion;
  unsigned framesWithDuplicatePair = 0;
  unsigned framesWithOtherSeriesPair = 0;
  for (const std::set<OFString>& refs : perFrame)
  {
    frameUnion.insert(refs.begin(), refs.end());
    REQUIRE(refs.count(uids[0]) == refs.count(duplicateUID));
    REQUIRE(refs.count(uids[1]) == refs.count(otherSeriesInstanceUID));
    if (refs.count(duplicateUID))
      framesWithDuplicatePair++;
    if (refs.count(otherSeriesInstanceUID))
      framesWithOtherSeriesPair++;
  }
  REQUIRE(framesWithDuplicatePair > 0);
  REQUIRE(framesWithOtherSeriesPair > 0);

  // Common Instance Reference module: every instance referenced by any frame,
  // grouped by the series it actually belongs to
  DcmSequenceOfItems* refSeriesSeq = nullptr;
  REQUIRE(seg->findAndGetSequence(DCM_ReferencedSeriesSequence, refSeriesSeq).good() && refSeriesSeq);
  REQUIRE(refSeriesSeq->card() == 2);

  std::set<OFString> cirUnion;
  std::map<OFString, std::set<OFString> > seriesToInstances;
  for (unsigned long s = 0; s < refSeriesSeq->card(); ++s)
  {
    OFString seriesUID;
    REQUIRE(refSeriesSeq->getItem(s)->findAndGetOFString(DCM_SeriesInstanceUID, seriesUID).good());
    DcmSequenceOfItems* refInstanceSeq = nullptr;
    REQUIRE(refSeriesSeq->getItem(s)->findAndGetSequence(DCM_ReferencedInstanceSequence, refInstanceSeq).good()
            && refInstanceSeq);
    for (unsigned long r = 0; r < refInstanceSeq->card(); ++r)
    {
      OFString uid;
      REQUIRE(refInstanceSeq->getItem(r)->findAndGetOFString(DCM_ReferencedSOPInstanceUID, uid).good());
      seriesToInstances[seriesUID].insert(uid);
      cirUnion.insert(uid);
    }
  }

  // the actual issue-554 assertion: nothing referenced by a frame may be
  // missing from the Common Instance Reference module (and vice versa)
  REQUIRE(cirUnion == frameUnion);
  REQUIRE(cirUnion.count(duplicateUID) == 1);

  // series grouping
  std::set<OFString> expectedFirstSeries = frameUnion;
  expectedFirstSeries.erase(otherSeriesInstanceUID);
  REQUIRE(seriesToInstances.count(firstSeriesUID) == 1);
  REQUIRE(seriesToInstances.count(otherSeriesUID) == 1);
  REQUIRE(seriesToInstances[firstSeriesUID] == expectedFirstSeries);
  REQUIRE(seriesToInstances[otherSeriesUID] == std::set<OFString>{otherSeriesInstanceUID});

  for (DcmItem* item : datasets) { delete item; }

  std::cout << "PASS: instances sharing a frame are all present in the Common Instance "
            << "Reference module, grouped by the series they belong to." << std::endl;
  return EXIT_SUCCESS;
}
