
// DCMQI includes
#include "dcmqi/Itk2DicomConverter.h"
#include "dcmqi/ColorUtilities.h"
#include "dcmqi/JSONSegmentationMetaInformationHandler.h"

// DCMTK includes
#include <dcmtk/config/osconfig.h>
#if defined(PACKAGE_VERSION_NUMBER) && PACKAGE_VERSION_NUMBER > 366
  #include <dcmtk/dcmdata/dcuid.h>
  #include <dcmtk/dcmdata/dcvrda.h>
  #include <dcmtk/dcmdata/dcvrtm.h>
  #include <dcmtk/dcmfg/fgfracon.h>
#endif
#include <dcmtk/dcmsr/codes/dcm.h>

#include <itkVectorImageToImageAdaptor.h>


namespace dcmqi {


  Itk2DicomConverter::Itk2DicomConverter()
  {
  };

  // -------------------------------------------------------------------------------------

  template<class ImageSourceType, std::enable_if_t<std::is_same<short, typename ImageSourceType::PixelType>::value, bool>>
  DcmDataset* Itk2DicomConverter::itkimage2dcmSegmentation(vector<DcmItem*> dcmDatasets,
                                                          vector<itk::SmartPointer<const ImageSourceType>> segmentations,
                                                          const string &metaData,
                                                          bool skipEmptySlices,
                                                          bool useLabelIDAsSegmentNumber,
                                                          bool referencesGeometryCheck) {

    auto inputSize = segmentations[0]->GetBufferedRegion().GetSize();

    JSONSegmentationMetaInformationHandler metaInfo(metaData.c_str());
    metaInfo.read();

    if(metaInfo.segmentsAttributesMappingList.size() != segmentations.size()){
      cerr << "Mismatch between the number of input segmentation files and the size of metainfo list!" << endl;
      return NULL;
    };

    IODGeneralEquipmentModule::EquipmentInfo eq = getEquipmentInfo();
    ContentIdentificationMacro ident = createContentIdentificationInformation(metaInfo);
    CHECK_COND(ident.setInstanceNumber(metaInfo.getInstanceNumber().c_str()));
    // Map that will hold the mapping from segment number (as written to DICOM) and label ID
    // as it is found in the input image. This can be used later to re-map the segment numbers
    // to their original label IDs (see mapLabelIDsToSegmentNumbers() method).
    map<Uint16,Uint16> segNum2Label;

    /* Create new segmentation document */
    DcmSegmentation *segdoc = NULL;

    DcmSegmentation::createBinarySegmentation(
        segdoc,   // resulting segmentation
        inputSize[1],    // rows
        inputSize[0],    // columns
        eq,     // equipment
        ident);   // content identification

    // import Patient, Study and Frame of Reference; do not import Series
    // attributes
    CHECK_COND(segdoc->importHierarchy(*dcmDatasets[0], OFTrue, OFTrue, OFTrue, OFFalse));

    /* Initialize dimension module */
    char dimUID[128];
    dcmGenerateUniqueIdentifier(dimUID, QIICR_UID_ROOT);
    IODMultiframeDimensionModule &mfdim = segdoc->getDimensions();
    CHECK_COND(mfdim.addDimensionIndex(DCM_ReferencedSegmentNumber, dimUID, DCM_SegmentIdentificationSequence,
                       DcmTag(DCM_ReferencedSegmentNumber).getTagName()));
    CHECK_COND(mfdim.addDimensionIndex(DCM_ImagePositionPatient, dimUID, DCM_PlanePositionSequence,
                       DcmTag(DCM_ImagePositionPatient).getTagName()));

    /* Initialize shared functional groups */
    const unsigned frameSize = inputSize[0] * inputSize[1];

    // Shared FGs: PlaneOrientationPatientSequence
    {
      auto labelDirMatrix = segmentations[0]->GetDirection();

      FGPlaneOrientationPatient *planor =
          FGPlaneOrientationPatient::createMinimal(
              Helper::floatToStr(labelDirMatrix[0][0]).c_str(),
              Helper::floatToStr(labelDirMatrix[1][0]).c_str(),
              Helper::floatToStr(labelDirMatrix[2][0]).c_str(),
              Helper::floatToStr(labelDirMatrix[0][1]).c_str(),
              Helper::floatToStr(labelDirMatrix[1][1]).c_str(),
              Helper::floatToStr(labelDirMatrix[2][1]).c_str());

      CHECK_COND(segdoc->addForAllFrames(*planor));
    }

    // Shared FGs: PixelMeasuresSequence
    {
      FGPixelMeasures *pixmsr = new FGPixelMeasures();

      auto labelSpacing = segmentations[0]->GetSpacing();
      ostringstream spacingSStream;
      spacingSStream << scientific << labelSpacing[1] << "\\" << labelSpacing[0];
      CHECK_COND(pixmsr->setPixelSpacing(spacingSStream.str().c_str()));

      spacingSStream.clear(); spacingSStream.str("");
      spacingSStream << scientific << labelSpacing[2];
      CHECK_COND(pixmsr->setSpacingBetweenSlices(spacingSStream.str().c_str()));
      CHECK_COND(pixmsr->setSliceThickness(spacingSStream.str().c_str()));
      CHECK_COND(segdoc->addForAllFrames(*pixmsr));
      delete pixmsr;
    }

    // Iterate over the files and labels available in each file, create a segment for each label,
    // initialize segment frames and add to the document

    OFString seriesInstanceUID, classUID;
    set<OFString> instanceUIDs;

    IODCommonInstanceReferenceModule &commref = segdoc->getCommonInstanceReference();
    OFVector<IODSeriesAndInstanceReferenceMacro::ReferencedSeriesItem*> &refseries = commref.getReferencedSeriesItems();

    IODSeriesAndInstanceReferenceMacro::ReferencedSeriesItem* refseriesItem = new IODSeriesAndInstanceReferenceMacro::ReferencedSeriesItem;

    OFVector<SOPInstanceReferenceMacro*> &refinstances = refseriesItem->getReferencedInstanceItems();

    CHECK_COND(dcmDatasets[0]->findAndGetOFString(DCM_SeriesInstanceUID, seriesInstanceUID));
    CHECK_COND(refseriesItem->setSeriesInstanceUID(seriesInstanceUID));

    // Shared FGs: DerivationImageSequence
    if(!referencesGeometryCheck && dcmDatasets.size() > 1){
      FGDerivationImage *fgder = new FGDerivationImage();
      DerivationImageItem *derimgItem;

      DSRBasicCodedEntry code_seg=CODE_DCM_Segmentation_113076;
      CHECK_COND(fgder->addDerivationImageItem(CodeSequenceMacro(code_seg.CodeValue,code_seg.CodingSchemeDesignator,
			code_seg.CodeMeaning),"",derimgItem));

      OFVector<SourceImageItem*> srcimgItems;
      OFVector<DcmItem*> dcmDatasets_ofvector;
      for (const auto& dataset : dcmDatasets) {
        dcmDatasets_ofvector.push_back(dataset);
      }
      derimgItem->addSourceImageItems(dcmDatasets_ofvector,
        CodeSequenceMacro(code_seg.CodeValue,code_seg.CodingSchemeDesignator, code_seg.CodeMeaning), srcimgItems, OFTrue /*skip file errors */);

      for(size_t src_image_cnt=0;src_image_cnt<srcimgItems.size();src_image_cnt++){
        // initialize class UID and series instance UID
        ImageSOPInstanceReferenceMacro &instRef = srcimgItems[src_image_cnt]->getImageSOPInstanceReference();
        OFString instanceUID;
        CHECK_COND(instRef.getReferencedSOPClassUID(classUID));
        CHECK_COND(instRef.getReferencedSOPInstanceUID(instanceUID));

        if(instanceUIDs.find(instanceUID) == instanceUIDs.end()){
          SOPInstanceReferenceMacro *refinstancesItem = new SOPInstanceReferenceMacro();
          CHECK_COND(refinstancesItem->setReferencedSOPClassUID(classUID));
          CHECK_COND(refinstancesItem->setReferencedSOPInstanceUID(instanceUID));
          refinstances.push_back(refinstancesItem);
          instanceUIDs.insert(instanceUID);
        }
      }
      CHECK_COND(segdoc->addForAllFrames(*fgder));
      delete fgder;
    }

    int uidfound = 0, uidnotfound = 0;
    Uint8 *frameData = new Uint8[frameSize];

    bool hasDerivationImages = false;

    FGPlanePosPatient* fgppp = FGPlanePosPatient::createMinimal("1","1","1");
    FGFrameContent* fgfc = new FGFrameContent();
    FGDerivationImage* fgder = new FGDerivationImage();
    OFVector<FGBase*> perFrameFGs;
    unsigned framesAdded = 0;

    for(size_t segFileNumber=0; segFileNumber<segmentations.size(); segFileNumber++){

      vector<vector<int> > slice2derimg;
      if(referencesGeometryCheck){
        slice2derimg = getSliceMapForSegmentation2DerivationImage(dcmDatasets, segmentations[segFileNumber]);
        for(vector<vector<int> >::const_iterator vI=slice2derimg.begin();vI!=slice2derimg.end();++vI)
          if((*vI).size()>0)
            hasDerivationImages = true;
      } else {
        hasDerivationImages = false;
      }

      perFrameFGs.clear();
      perFrameFGs.push_back(fgppp);
      perFrameFGs.push_back(fgfc);
      if(hasDerivationImages)
        perFrameFGs.push_back(fgder);

      // note that labels are the same in the input and output image produced
      // by this filter, see https://itk.org/Doxygen/html/classitk_1_1LabelImageToLabelMapFilter.html
      using LabelToLabelMapFilterType2 = itk::LabelImageToLabelMapFilter<ImageSourceType>;
      typename LabelToLabelMapFilterType2::Pointer l2lm = LabelToLabelMapFilterType2::New();
      l2lm->SetInput(segmentations[segFileNumber]);
      l2lm->Update();

      typedef typename LabelToLabelMapFilterType2::OutputImageType::LabelObjectType LabelType;
      typedef itk::LabelStatisticsImageFilter<ImageSourceType,ImageSourceType> LabelStatisticsType;

      typename LabelStatisticsType::Pointer labelStats = LabelStatisticsType::New();

      cout << "Found " << l2lm->GetOutput()->GetNumberOfLabelObjects() << " label(s)" << endl;
      labelStats->SetInput(segmentations[segFileNumber]);
      labelStats->SetLabelInput(segmentations[segFileNumber]);
      labelStats->Update();

      bool cropSegmentsBBox = false;
      if(cropSegmentsBBox){
        cout << "WARNING: Crop operation enabled - WIP" << endl;
        typedef itk::BinaryThresholdImageFilter<ImageSourceType,ImageSourceType> ThresholdType;
        typename ThresholdType::Pointer thresh = ThresholdType::New();
        thresh->SetInput(segmentations[segFileNumber]);
        thresh->SetLowerThreshold(1);
        thresh->SetLowerThreshold(100);
        thresh->SetInsideValue(1);
        thresh->Update();

        typename LabelStatisticsType::Pointer threshLabelStats = LabelStatisticsType::New();

        threshLabelStats->SetInput(thresh->GetOutput());
        threshLabelStats->SetLabelInput(thresh->GetOutput());
        threshLabelStats->Update();

        auto threshBbox = threshLabelStats->GetBoundingBox(1);
        /*
        cout << "OVerall bounding box: " << threshBbox[0] << ", " << threshBbox[1]
               << threshBbox[2] << ", " << threshBbox[3]
               << threshBbox[4] << ", " << threshBbox[5]
               << endl;
               */
        return NULL;
      }

      for(unsigned segLabelNumber=0 ; segLabelNumber<l2lm->GetOutput()->GetNumberOfLabelObjects();segLabelNumber++){
        LabelType* labelObject = l2lm->GetOutput()->GetNthLabelObject(segLabelNumber);
        short label = labelObject->GetLabel();

        if(!label){
          continue;
        }

        cout << "Processing label " << label << endl;

        auto bbox = labelStats->GetBoundingBox(label);
        unsigned firstSlice, lastSlice;
        //bool skipEmptySlices = true; // TODO: what to do with that line?
        //bool skipEmptySlices = false; // TODO: what to do with that line?
        if(skipEmptySlices){
          firstSlice = bbox[4];
          lastSlice = bbox[5]+1;
        } else {
          firstSlice = 0;
          lastSlice = inputSize[2];
        }

        cout << "Total non-empty slices that will be encoded in SEG for label " <<
        label << " is " << lastSlice-firstSlice+1 << endl <<
        " (inclusive from " << firstSlice << " to " <<
        lastSlice << ")" << endl;

        DcmSegment* segment = NULL;
        if(metaInfo.segmentsAttributesMappingList[segFileNumber].find(label) == metaInfo.segmentsAttributesMappingList[segFileNumber].end()){
          cerr << "ERROR: Failed to match label from image to the segment metadata!" << endl;
          return NULL;
        }

        SegmentAttributes* segmentAttributes = metaInfo.segmentsAttributesMappingList[segFileNumber][label];

        DcmSegTypes::E_SegmentAlgoType algoType = DcmSegTypes::SAT_UNKNOWN;
        string algoName = "";
        string algoTypeStr = segmentAttributes->getSegmentAlgorithmType();
        if(algoTypeStr == "MANUAL"){
          algoType = DcmSegTypes::SAT_MANUAL;
        } else {
          if(algoTypeStr == "AUTOMATIC")
            algoType = DcmSegTypes::SAT_AUTOMATIC;
          if(algoTypeStr == "SEMIAUTOMATIC")
            algoType = DcmSegTypes::SAT_SEMIAUTOMATIC;

          algoName = segmentAttributes->getSegmentAlgorithmName();
          if(algoName == ""){
            cerr << "ERROR: Algorithm name must be specified for non-manual algorithm types!" << endl;
            return NULL;
          }
        }

        CodeSequenceMacro* typeCode = segmentAttributes->getSegmentedPropertyTypeCodeSequence();
        CodeSequenceMacro* categoryCode = segmentAttributes->getSegmentedPropertyCategoryCodeSequence();
        assert(typeCode != NULL && categoryCode!= NULL);
        OFString segmentLabel;

        if(segmentAttributes->getSegmentLabel().length() > 0){
          cout << "Populating segment label to " << segmentAttributes->getSegmentLabel() << endl;
          segmentLabel = segmentAttributes->getSegmentLabel().c_str();
        } else if(segmentAttributes->getSegmentDescription().length() > 0){
          cout << "Populating segment label from SegmentDescription to " << segmentAttributes->getSegmentDescription() << endl;
          segmentLabel = segmentAttributes->getSegmentDescription().c_str();
        } else
          CHECK_COND(typeCode->getCodeMeaning(segmentLabel));

        CHECK_COND(DcmSegment::create(segment, segmentLabel, *categoryCode, *typeCode, algoType, algoName.c_str()));

        if(segmentAttributes->getSegmentDescription().length() > 0)
          segment->setSegmentDescription(segmentAttributes->getSegmentDescription().c_str());

        if(segmentAttributes->getTrackingIdentifier().length() > 0)
          segment->setTrackingID(segmentAttributes->getTrackingIdentifier().c_str());

        if(segmentAttributes->getTrackingUniqueIdentifier().length() > 0)
          segment->setTrackingUID(segmentAttributes->getTrackingUniqueIdentifier().c_str());

        CodeSequenceMacro* typeModifierCode = segmentAttributes->getSegmentedPropertyTypeModifierCodeSequence();
        if (typeModifierCode != NULL) {
          OFVector<CodeSequenceMacro*>& modifiersVector = segment->getSegmentedPropertyTypeModifierCode();
          modifiersVector.push_back(typeModifierCode);
        }

        GeneralAnatomyMacro &anatomyMacro = segment->getGeneralAnatomyCode();
        if (segmentAttributes->getAnatomicRegionSequence() != NULL){
          OFVector<CodeSequenceMacro*>& anatomyMacroModifiersVector = anatomyMacro.getAnatomicRegionModifier();
          CodeSequenceMacro& anatomicRegionSequence = anatomyMacro.getAnatomicRegion();
          anatomicRegionSequence = *segmentAttributes->getAnatomicRegionSequence();

          if(segmentAttributes->getAnatomicRegionModifierSequence() != NULL){
            CodeSequenceMacro* anatomicRegionModifierSequence = segmentAttributes->getAnatomicRegionModifierSequence();
            anatomyMacroModifiersVector.push_back(anatomicRegionModifierSequence);
          }
        }

        unsigned* rgb = segmentAttributes->getRecommendedDisplayRGBValue();
        int cielab[3];

        ColorUtilities::getIntegerScaledCIELabPCSFromSRGB(cielab[0], cielab[1], cielab[2], rgb[0], rgb[1], rgb[2]);
        //IODCIELabUtil::rgb2DicomLab(cielab[0], cielab[1], cielab[2], rgb[0], rgb[1], rgb[2]);

        CHECK_COND(segment->setRecommendedDisplayCIELabValue(cielab[0],cielab[1],cielab[2]));

        Uint16 segmentNumber;
        CHECK_COND(segdoc->addSegment(segment, segmentNumber /* returns logical segment number */));
        segNum2Label.insert(make_pair(segmentNumber, label));

        // TODO: make it possible to skip empty frames (optional)
        // iterate over slices for an individual label and populate output frames
        for(unsigned sliceNumber=firstSlice;sliceNumber<lastSlice;sliceNumber++){

          // PerFrame FG: FrameContentSequence
          //fracon->setStackID("1"); // all frames go into the same stack
          CHECK_COND(fgfc->setDimensionIndexValues(segmentNumber, 0));
          CHECK_COND(fgfc->setDimensionIndexValues(sliceNumber-firstSlice+1, 1));
          //ostringstream inStackPosSStream; // StackID is not present/needed
          //inStackPosSStream << s+1;
          //fracon->setInStackPositionNumber(s+1);

          // PerFrame FG: PlanePositionSequence
          {
            typename ImageSourceType::PointType sliceOriginPoint;
            typename ImageSourceType::IndexType sliceOriginIndex;
            sliceOriginIndex.Fill(0);
            sliceOriginIndex[2] = sliceNumber;
            segmentations[segFileNumber]->TransformIndexToPhysicalPoint(sliceOriginIndex, sliceOriginPoint);
            ostringstream pppSStream;
            if(sliceNumber>0){
              typename ImageSourceType::PointType prevOrigin;
              typename ImageSourceType::IndexType prevIndex;
              prevIndex.Fill(0);
              prevIndex[2] = sliceNumber-1;
              segmentations[segFileNumber]->TransformIndexToPhysicalPoint(prevIndex, prevOrigin);
            }
            fgppp->setImagePositionPatient(
                Helper::floatToStr(sliceOriginPoint[0]).c_str(),
                Helper::floatToStr(sliceOriginPoint[1]).c_str(),
                Helper::floatToStr(sliceOriginPoint[2]).c_str());
          }

          /* Add frame that references this segment */
          {
            typename ImageSourceType::RegionType sliceRegion;
            typename ImageSourceType::IndexType sliceIndex;
            typename ImageSourceType::SizeType sliceSize;

            sliceIndex[0] = 0;
            sliceIndex[1] = 0;
            sliceIndex[2] = sliceNumber;

            sliceSize[0] = inputSize[0];
            sliceSize[1] = inputSize[1];
            sliceSize[2] = 1;

            sliceRegion.SetIndex(sliceIndex);
            sliceRegion.SetSize(sliceSize);

            unsigned framePixelCnt = 0;
            itk::ImageRegionConstIteratorWithIndex<ImageSourceType> sliceIterator(segmentations[segFileNumber], sliceRegion);
            for(sliceIterator.GoToBegin();!sliceIterator.IsAtEnd();++sliceIterator,++framePixelCnt){
              if(sliceIterator.Get() == label){
                frameData[framePixelCnt] = 1;
                auto idx = sliceIterator.GetIndex();
              } else
                frameData[framePixelCnt] = 0;
            }

            OFVector<DcmItem*> siVector;
            if(referencesGeometryCheck){
              for(size_t derImageInstanceNum=0;
                  derImageInstanceNum<slice2derimg[sliceNumber].size();
                  derImageInstanceNum++){
                siVector.push_back(dcmDatasets[slice2derimg[sliceNumber][derImageInstanceNum]]);
              }

              if(siVector.size()>0){

                DerivationImageItem *derimgItem;
          DSRBasicCodedEntry code_seg=CODE_DCM_Segmentation_113076;
                CHECK_COND(fgder->addDerivationImageItem(CodeSequenceMacro(code_seg.CodeValue,code_seg.CodingSchemeDesignator,
            code_seg.CodeMeaning),"",derimgItem));

          DSRBasicCodedEntry code = CODE_DCM_SourceImageForImageProcessingOperation;
                OFVector<SourceImageItem*> srcimgItems;
                CHECK_COND(derimgItem->addSourceImageItems(siVector,
                                                        CodeSequenceMacro(code.CodeValue, code.CodingSchemeDesignator,
                                code.CodeMeaning),
                                                        srcimgItems));

                {
                  // initialize class UID and series instance UID
                  ImageSOPInstanceReferenceMacro &instRef = srcimgItems[0]->getImageSOPInstanceReference();
                  OFString instanceUID;
                  CHECK_COND(instRef.getReferencedSOPClassUID(classUID));
                  CHECK_COND(instRef.getReferencedSOPInstanceUID(instanceUID));

                  if(instanceUIDs.find(instanceUID) == instanceUIDs.end()){
                    SOPInstanceReferenceMacro *refinstancesItem = new SOPInstanceReferenceMacro();
                    CHECK_COND(refinstancesItem->setReferencedSOPClassUID(classUID));
                    CHECK_COND(refinstancesItem->setReferencedSOPInstanceUID(instanceUID));
                    refinstances.push_back(refinstancesItem);
                    instanceUIDs.insert(instanceUID);
                    uidnotfound++;
                  } else {
                    uidfound++;
                  }
                }
              }
            }

            OFCondition frameAdded = segdoc->addFrame(frameData, segmentNumber, perFrameFGs);
            if(frameAdded.good()){
              framesAdded++;
            }

            // remove derivation image FG from the per-frame FGs, only if applicable!
            if(referencesGeometryCheck && siVector.size()>0){
              // clean up for the next frame
              fgder->clearData();
            }
          }
        }
      }
    }

    if(framesAdded == 0){
      cerr << "FATAL ERROR: No input labels found - input segmentation is empty!" << endl;
      cerr << "If you would like to encode background label, please see https://github.com/QIICR/dcmqi/issues/490" << endl;
      return NULL;
    }

    // add ReferencedSeriesItem only if it is not empty
    if(refinstances.size())
      refseries.push_back(refseriesItem);

    delete fgfc;
    delete fgppp;
    if(referencesGeometryCheck)
      delete fgder;

    segdoc->getSeries().setSeriesNumber(metaInfo.getSeriesNumber().c_str());

    OFString frameOfRefUID;
    if(!segdoc->getFrameOfReference().getFrameOfReferenceUID(frameOfRefUID).good()){
      // TODO: add FoR UID to the metadata JSON and check that before generating one!
      char frameOfRefUIDchar[128];
      dcmGenerateUniqueIdentifier(frameOfRefUIDchar, QIICR_UID_ROOT);
      CHECK_COND(segdoc->getFrameOfReference().setFrameOfReferenceUID(frameOfRefUIDchar));
    }

    std::cout << "Writing DICOM segmentation dataset" << std::endl;
    // Don't check functional groups since its very time consuming and we trust
    // ourselves to put together valid datasets
    segdoc->setCheckFGOnWrite(OFFalse);

    // Ensure dataset memory is cleaned up on exit, and avoid the copy on
    // return that was performed before
    std::unique_ptr<DcmDataset> segdocDataset(new DcmDataset());
    OFCondition writeResult = segdoc->writeDataset(*segdocDataset);
    if(writeResult.bad()){
      cerr << "FATAL ERROR: Writing of the SEG dataset failed!";
      if (writeResult.text()){
        cerr << " Error: " << writeResult.text() << ".";
      }
      cerr << " Please report the problem to the developers, ideally accompanied by a de-identified dataset allowing to reproduce the problem!" << endl;
      return NULL;
    }

    // Set reader/session/timepoint information
    std::cout << "Patching in extra meta information into DICOM dataset" << std::endl;
    CHECK_COND(segdocDataset->putAndInsertString(DCM_SeriesDescription, metaInfo.getSeriesDescription().c_str()));
    CHECK_COND(segdocDataset->putAndInsertString(DCM_ContentCreatorName, metaInfo.getContentCreatorName().c_str()));
    CHECK_COND(segdocDataset->putAndInsertString(DCM_ClinicalTrialSeriesID, metaInfo.getClinicalTrialSeriesID().c_str()));
    CHECK_COND(segdocDataset->putAndInsertString(DCM_ClinicalTrialTimePointID, metaInfo.getClinicalTrialTimePointID().c_str()));
    if (metaInfo.getClinicalTrialCoordinatingCenterName().size())
      CHECK_COND(segdocDataset->putAndInsertString(DCM_ClinicalTrialCoordinatingCenterName, metaInfo.getClinicalTrialCoordinatingCenterName().c_str()));

    // populate BodyPartExamined
    {
      OFString bodyPartStr;
      string bodyPartAssigned = metaInfo.getBodyPartExamined();

      // inherit BodyPartExamined from the source image dataset, if available
      if(dcmDatasets[0]->findAndGetOFString(DCM_BodyPartExamined, bodyPartStr).good())
      if(string(bodyPartStr.c_str()).size())
        bodyPartAssigned = bodyPartStr.c_str();

      if(bodyPartAssigned.size())
        CHECK_COND(segdocDataset->putAndInsertString(DCM_BodyPartExamined, bodyPartAssigned.c_str()));
    }

    // StudyDate/Time should be of the series segmented, not when segmentation was made - this is initialized by DCMTK

    // SeriesDate/Time should be of when segmentation was done; initialize to when it was saved
    {
      OFString contentDate, contentTime;
      DcmDate::getCurrentDate(contentDate);
      DcmTime::getCurrentTime(contentTime);

      CHECK_COND(segdocDataset->putAndInsertString(DCM_SeriesDate, contentDate.c_str()));
      CHECK_COND(segdocDataset->putAndInsertString(DCM_SeriesTime, contentTime.c_str()));
      segdoc->getGeneralImage().setContentDate(contentDate.c_str());
      segdoc->getGeneralImage().setContentTime(contentTime.c_str());
    }

    {
      string segmentsOverlap;
      if(segmentations.size() == 1)
        segmentsOverlap = "NO";
      else
        segmentsOverlap = "UNDEFINED";
      CHECK_COND(segdocDataset->putAndInsertString(DCM_SegmentsOverlap, segmentsOverlap.c_str()));
    }

    if (useLabelIDAsSegmentNumber)
    {
      mapLabelIDsToSegmentNumbers(segdocDataset.get(), segNum2Label);
    }

    return segdocDataset.release();
  }


  bool Itk2DicomConverter::mapLabelIDsToSegmentNumbers(DcmDataset* dset, map<Uint16,Uint16> segNum2Label)
  {
    cout << "Mapping Label IDs to Segment Numbers" << endl;
    DcmSequenceOfItems* seq = NULL;
    CHECK_COND(dset->findAndGetSequence(DCM_PerFrameFunctionalGroupsSequence, seq));
    if(!seq){
      cerr << "ERROR: Mapping Label IDs to Segment Numbers: Per-frame functional groups sequence not found!" << endl;
      return false;
    }

    // Check whether the provided label numbers (values in segNum2Label) are unique
    // and monotonically increasing from 1.
    if (!checkLabelNumbering(segNum2Label))
    {
      return false;
    }

    // 0. Get Segment Sequence and remember number of segments
    // 1. Loop over all items in per-frame FG sequence
    // 2. Get segmentation FG for that frame
    // 3. Get the referenced segment number from it
    // 4. Replace the referenced segment number with the original label value
    // 5. From the Segment Sequence, find the item with the matching segment number
    // 6. Replace the segment number in the item of the Segment Sequence to make it match the replacement (label) value
    // 7. Sort items in Segment Sequence by newly assigned Segment Numbers

    // 0. Get Segment Sequence
    DcmSequenceOfItems* segmentSequence = NULL;
    CHECK_COND(dset->findAndGetSequence(DCM_SegmentSequence, segmentSequence));
    if(!segmentSequence){
      cerr << "ERROR: Mapping Label IDs to Segment Numbers: Segment sequence not found!" << endl;
      return false;
    }
    // This tells us how many segments we have, and therefore also how many segment numbers we must expect
    Uint16 numSegments = segmentSequence->card();
    // Vector that holds all input segments (from Segment Sequence) that have been handled. This is
    // used to check whether the Segmentation Sequence has already been updated with the new segment number.
    // Since multiple frames can reference the same segment and we are looping over frames,
    // we only want to update the segment sequence once per segment to save time.
    // The index is the sequence number, the value is the item from the Segment Sequence.
    vector<DcmItem*> segmentHandled(numSegments, NULL);

    // 1.Get per-frame FG sequence and loop over all items (frames)
    DcmSequenceOfItems* fgSeq = NULL;
    CHECK_COND(dset->findAndGetSequence(DCM_PerFrameFunctionalGroupsSequence, fgSeq));
    for(unsigned i=0;i<fgSeq->card();i++){
      DcmItem* frameItem = seq->getItem(i);
      if(!frameItem){
        cerr << "ERROR: Mapping Label IDs to Segment Numbers: Failed to get item " << i << " from the per-frame FG sequence!" << endl;
        return false;
      }

      // 2. Get segmentation FG for that frame
      DcmItem* segItem = NULL;
      CHECK_COND(frameItem->findAndGetSequenceItem(DCM_SegmentIdentificationSequence, segItem, 0));

      // 3. Get the referenced segment number from it
      Uint16 oldSegNum;
      CHECK_COND(segItem->findAndGetUint16(DCM_ReferencedSegmentNumber, oldSegNum));

      // 4. Replace the referenced segment number with the original label value
      map<Uint16,Uint16>::iterator segNum2LabelIt = segNum2Label.find(oldSegNum);
      if(segNum2LabelIt == segNum2Label.end()){
        cerr << "ERROR: Mapping Label IDs to Segment Numbers: Failed to find segment number " << oldSegNum << " in the mapping!" << endl;
        return false;
      }
      Uint16 newSegNum = segNum2LabelIt->second;
      CHECK_COND(segItem->putAndInsertUint16(DCM_ReferencedSegmentNumber, newSegNum));
      // Replace the referenced segment number with the original label value if not already done.
      // Range check is not necessary, because we already checked sanity of segmentSequence.
      if (!segmentHandled[newSegNum-1])
      {
        // 5. From the Segment Sequence, find the item with the matching old segment number
        DcmItem* segmentItem = NULL;
        for(unsigned j=0;j<segmentSequence->card();j++){
          segmentItem = segmentSequence->getItem(j);
          if(!segmentItem){
            cerr << "ERROR: Mapping Label IDs to Segment Numbers: Failed to get item " << j << " from the segment sequence!" << endl;
            return false;
          }
          Uint16 segmentNumber;
          CHECK_COND(segmentItem->findAndGetUint16(DCM_SegmentNumber, segmentNumber));
          // 6. Replace the segment number in the item of the Segment Sequence to make it match the replacement (label) value
          if(segmentNumber == oldSegNum)
          {
            segmentHandled[newSegNum-1] = new DcmItem(*segmentItem);
            CHECK_COND(segmentHandled[newSegNum-1]->putAndInsertUint16(DCM_SegmentNumber,newSegNum));
            break;
          }
        }
        if(!segmentItem){
          cerr << "ERROR: Mapping Label IDs to Segment Numbers: Failed to find segment number " << oldSegNum << " in the segment sequence!" << endl;
          return false;
        }
      }
    }
    // 7. Sort items in Segment Sequence by newly assigned Segment Numbers
    std::unique_ptr<DcmSequenceOfItems> newSegSeq(new DcmSequenceOfItems(DCM_SegmentSequence));
    for (size_t z=0; z<segmentHandled.size(); z++)
    {
      if (!segmentHandled[z])
      {
        cerr << "ERROR: Mapping Label IDs to Segment Numbers: Segment number " << z+1 << " is missing!" << endl;
        return false;
      }
      newSegSeq->insert(segmentHandled[z]);
    }
    dset->insert(newSegSeq.release(), OFTrue /* replace old */);
    return true;
  }

  bool Itk2DicomConverter::checkLabelNumbering(const map<Uint16, Uint16>& segNum2Label)
  {
    // Check whether the provided label numbers (values in segNum2Label) are unique
    // and monotonically increasing from 1.
    // If not, we cannot use this method to sort the segments.
    std::vector<Uint16> labels;
    for (auto it = segNum2Label.begin(); it != segNum2Label.end(); ++it)
    {
      labels.push_back(it->second);
    }
    std::sort(labels.begin(), labels.end());
    if (labels[0] != 1)
    {
      cerr << "ERROR: Cannot sort segments by label, because the label numbers are not monotonically increasing from 1!" << endl;
      return false;
    }
    for (size_t i=0; i<labels.size(); i++)
    {
      if (labels[i] != i+1)
      {
        cerr << "ERROR: Cannot sort segments by label, because the label numbers are not monotonically increasing from 1!" << endl;
        return false;
      }
    }
    return true;
  }

  template DcmDataset* Itk2DicomConverter::itkimage2dcmSegmentation<ShortImageType>(
      vector<DcmItem*> dcmDatasets,
      vector<ShortImageType::ConstPointer> segmentations,
      const string& metaData,
      bool skipEmptySlices,
      bool useLabelIDAsSegmentNumber,
      bool referencesGeometryCheck);

  using VectorImageAdapter = itk::VectorImageToImageAdaptor<short, 3U>;
  template DcmDataset* Itk2DicomConverter::itkimage2dcmSegmentation<VectorImageAdapter>(
      vector<DcmItem*> dcmDatasets,
      vector<VectorImageAdapter::ConstPointer> segmentations,
      const string& metaData,
      bool skipEmptySlices,
      bool useLabelIDAsSegmentNumber,
      bool referencesGeometryCheck);
}
