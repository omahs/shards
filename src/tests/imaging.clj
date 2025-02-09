; SPDX-License-Identifier: BSD-3-Clause
; Copyright © 2020 Fragcolor Pte. Ltd.

(def Root (Mesh))

(schedule
 Root
 (Wire
  "test"
  (LoadImage "../../assets/simple1.PNG") >= .baseImg
  (StripAlpha)
  (WritePNG "testbase.png")
  (Ref .img)
  (Repeat (->
           (Get .img)
           (Convolve 50)
           (WritePNG "test.png")
           (Log)
           (ImageToFloats)
           (Ref .s)
           (Count .s)
           (Log)
           (Get .s)
           (FloatsToImage 99 99 3)
           (WritePNG "test2.png")
           (FillAlpha)
           (ResizeImage 200 200)
           (WritePNG "test2Resized.png"))
          30)
  (Log)
  .baseImg
  (ResizeImage :Width 200 :Height 0)
  (WritePNG "testResized1.png")
  .baseImg
  (ResizeImage :Width 0 :Height 200)
  (WritePNG "testResized2.png")
  (WritePNG) (ExpectBytes) ;
  (LoadImage) (ImageToBytes) (ExpectBytes) ;
  ))

(run Root 0.1)
