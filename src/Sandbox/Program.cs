using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Threading.Tasks;
using WPFAnimationEncoding;

namespace Sandbox
{
    class Program
    {
        static void Main(string[] args)
        {
            using (var file = new VideoReEncoder())
            {
                var outputFile = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "h264out.mp4");
                if (File.Exists(outputFile))
                    File.Delete(outputFile);

                var inputFile = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "h264.mp4");

                var sandbox = new WPFAnimationEncoding.Sandbox();
                sandbox.Test(inputFile, outputFile);

                //var fileReader = new WPFAnimationEncoding.VideoFileReader();
                //fileReader.Open(inputFile);
                //var fileWriter = new WPFAnimationEncoding.VideoFileWriter();
                //fileWriter.Open(outputFile, 640, 360, fileReader.FrameRate, VideoCodec.Default);

                //var frame = fileReader.ReadVideoFrame();

                //while (frame != null)
                //{
                //    fileWriter.WriteVideoFrame(frame);
                //    frame = fileReader.ReadVideoFrame();
                //}

                //fileReader.Close();
                //fileWriter.Close();

                //var frameNumber = 0;

                //file.StartReEncoding(, 
                //    outputFile,
                //    (Bitmap image, ref bool cancel) =>
                //    {
                //        frameNumber++;

                //        using (var g = Graphics.FromImage(image))
                //        {
                //            g.SmoothingMode = SmoothingMode.AntiAlias;
                //            g.InterpolationMode = InterpolationMode.HighQualityBicubic;
                //            g.PixelOffsetMode = PixelOffsetMode.HighQuality;
                //            g.DrawString("Frame number " + frameNumber, new Font("Tahoma", 24), Brushes.Black,
                //                new PointF(0, 0));

                //            g.Flush();
                //            g.Dispose();
                //        }

                //        //var destination = Path.Combine(AppDomain.CurrentDomain.BaseDirectory,
                //        //    "test" + frameNumber + ".bmp");
                //        //if(File.Exists(destination))
                //        //    File.Delete(destination);

                //        //image.Save(destination);

                //        return image;
                //    });
            }
            Console.ReadLine();
        }
    }
}
