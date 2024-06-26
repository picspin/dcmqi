[![OpenHub](https://www.openhub.net/p/dcmqi/widgets/project_thin_badge.gif)](https://www.openhub.net/p/dcmqi) [![codecov](https://codecov.io/gh/QIICR/dcmqi/branch/master/graph/badge.svg)](https://codecov.io/gh/QIICR/dcmqi)
[![](https://img.shields.io/docker/pulls/qiicr/dcmqi.svg?maxAge=604800)](https://hub.docker.com/r/qiicr/dcmqi)

| CI Build     | Linux | Windows | Mac |
|--------------|-------|---------|-----|
| Build Status for latest | ![GitHub CI Linux](https://github.com/QIICR/dcmqi/actions/workflows/cmake-linux.yml/badge.svg) | ![GitHub CI Windows](https://github.com/QIICR/dcmqi/actions/workflows/cmake-win.yml/badge.svg) | ![GitHub CI Mac](https://github.com/QIICR/dcmqi/actions/workflows/cmake-macos.yml/badge.svg)


# Introduction

dcmqi (DICOM (**dcm**) for Quantitative Imaging (**qi**)) is a collection of libraries and command line tools with minimum dependencies to support standardized communication of [quantitative image analysis](http://journals.sagepub.com/doi/pdf/10.1177/0962280214537333) research data using [DICOM standard](https://en.wikipedia.org/wiki/DICOM).

Specifically, dcmqi can help you with the conversion of the following data types to and from DICOM:
* voxel-based segmentations using [DICOM Segmentation IOD](http://dicom.nema.org/medical/dicom/current/output/chtml/part03/sect_A.51.html)
* parametric maps using [DICOM Parametric map IOD](http://dicom.nema.org/medical/dicom/current/output/chtml/part03/sect_A.75.html)
* image-based measurements using [DICOM Structured Reporting (SR) template TID1500](http://dicom.nema.org/medical/dicom/current/output/chtml/part16/chapter_A.html#sect_TID_1500)

As an introduction to the motivation, capabilities and advantages of using the DICOM standard, and the objects mentioned above, you might want to read this open access paper:

> Fedorov A, Clunie D, Ulrich E, Bauer C, Wahle A, Brown B, Onken M, Riesmeier J, Pieper S, Kikinis R, Buatti J, Beichel RR. (2016) DICOM for quantitative imaging biomarker development: a standards based approach to sharing clinical data and structured PET/CT analysis results in head and neck cancer research. *PeerJ* 4:e2057 https://doi.org/10.7717/peerj.2057

dcmqi is developed and maintained by the [NCI Imaging Data Commons](https://imaging.datacommons.cancer.gov) project.

# Getting started

* install as easy as `pip install dcmqi`
* for alternative installation and usage instructions see [dcmqi manual](https://qiicr.gitbook.io/dcmqi-guide/).
* check out our [introductory tutorial](http://qiicr.org/dcmqi-guide/tutorials/intro.html)

# License

dcmqi is distributed under [3-clause BSD license](https://github.com/QIICR/dcmqi/blob/master/LICENSE.txt).

Our goal is to support and encourage adoption of the DICOM standard in both academic and commercial tools. We will be happy to hear about your usage of dcmqi, but you don't have to report back to us.

# Support

You can communicate you feedback, feature requests, comments or problem reports using any of the methods below:
* [submit issue](https://github.com/QIICR/dcmqi/issues/new) on dcmqi bug tracker
* post a question to [dcmqi google
  group](https://groups.google.com/forum/#!forum/dcmqi)

# Acknowledgments

To acknowledge dcmqi in an academic paper, please cite

> Herz C, Fillion-Robin J-C, Onken M, Riesmeier J, Lasso A, Pinter C, Fichtinger G, Pieper S, Clunie D, Kikinis R, Fedorov A.  _dcmqi: An Open Source Library for Standardized Communication of Quantitative Image Analysis Results Using DICOM_. *Cancer Research*. 2017;77(21):e87–e90 http://cancerres.aacrjournals.org/content/77/21/e87.

If you like dcmqi, please give the dcmqi repository [a star on github](https://help.github.com/articles/about-stars/). This is an easy way to show thanks and it can help us qualify for useful services that are only open to widely recognized open projects.

This project has been supported in part by the following funded initiatives:
* [National Cancer Institute Imaging Data Commons (IDC)](https://imaging.datacommons.cancer.gov) project, funded in whole or in part with Federal funds from the National Cancer Institute, National Institutes of Health, under Task Order No. HHSN26110071 under Contract No. HHSN261201500003l
* National Institutes of Health, National Cancer Institute, [Informatics Technology for Cancer Research (ITCR) program](https://itcr.nci.nih.gov/), grant [Quantitative Image Informatics for Cancer Research (QIICR)](http://qiicr.org) (U24 CA180918, PIs Kikinis and Fedorov)
* [Neuroimage Analysis Center (NAC)](http://nac.spl.harvard.edu/) (P41 EB015902, PI Kikinis)
* [National Center for Image Guided Therapy (NCIGT)](http://ncigt.org) (P41 EB015898, PI Tempany)

# References

1. Fedorov, A., Longabaugh, W. J. R., Pot, D., Clunie, D. A., Pieper, S., Aerts, H. J. W. L., Homeyer, A., Lewis, R., Akbarzadeh, A., Bontempi, D., Clifford, W., Herrmann, M. D., Höfener, H., Octaviano, I., Osborne, C., Paquette, S., Petts, J., Punzo, D., Reyes, M., Schacherer, D. P., Tian, M., White, G., Ziegler, E., Shmulevich, I., Pihl, T., Wagner, U., Farahani, K. & Kikinis, R. NCI Imaging Data Commons. Cancer Res. 81, 4188–4193 (2021). https://dx.doi.org/10.1158/0008-5472.CAN-21-0950

2. Fedorov A, Clunie D, Ulrich E, Bauer C, Wahle A, Brown B, Onken M, Riesmeier J, Pieper S, Kikinis R, Buatti J, Beichel RR. (2016) _DICOM for quantitative imaging biomarker development: a standards based approach to sharing clinical data and structured PET/CT analysis results in head and neck cancer research._ *PeerJ* 4:e2057 https://doi.org/10.7717/peerj.2057

3. Herz C, Fillion-Robin J-C, Onken M, Riesmeier J, Lasso A, Pinter C, Fichtinger G, Pieper S, Clunie D, Kikinis R, Fedorov A.  _dcmqi: An Open Source Library for Standardized Communication of Quantitative Image Analysis Results Using DICOM_. *Cancer Research*. 2017;77(21):e87–e90 http://cancerres.aacrjournals.org/content/77/21/e87.
