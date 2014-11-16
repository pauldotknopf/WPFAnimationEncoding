using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.IO;
using WPFAnimationEncoding;

namespace Sandbox
{
    class Program
    {
        private static string _outputFile;
        private static string _inputFile;

        static void Main()
        {
            _outputFile = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "h264out.mov");
            if (File.Exists(_outputFile))
                File.Delete(_outputFile);

            _inputFile = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "h264.mov");

            ReEncoderTests();
            //FileReaderTests();
            //FileWriterTests();

            Console.ReadLine();
        }

        static void SandboxTest()
        {
            var sandbox = new WPFAnimationEncoding.Sandbox();
            sandbox.Test(_inputFile, _outputFile);
        }

        static void FileWriterTests()
        {
            var fileReader = new WPFAnimationEncoding.VideoFileReader();
            fileReader.Open(_inputFile);
            var fileWriter = new WPFAnimationEncoding.VideoFileWriter();
            fileWriter.Open(_outputFile, 640, 360, fileReader.FrameRate, VideoCodec.Default, 30000000);

            var frame = fileReader.ReadVideoFrame();

            int frameNumber = 0;
            while (frame != null)
            {
                //var seconds = Math.Floor(frameNumber/(fileReader.FrameRate + 1));
                //var milliseconds = ((frameNumber % (fileReader.FrameRate + 1))*1000)/(fileReader.FrameRate + 1);
                //var timestamp = TimeSpan.FromSeconds(seconds);
                //timestamp = timestamp.Add(TimeSpan.FromMilliseconds(milliseconds));
                //Console.WriteLine("Frame number " + frameNumber + " is at " + timestamp.ToString());
                fileWriter.WriteVideoFrame(frame, frameNumber);
                frameNumber++;
                frame = fileReader.ReadVideoFrame();
            }

            fileReader.Close();
            fileWriter.Close();
        }

        static void FileReaderTests()
        {
            var fileReader = new WPFAnimationEncoding.VideoFileReader();
            fileReader.Open(_inputFile);

            var frame = fileReader.ReadVideoFrame();
            while (frame != null)
            {
                frame = fileReader.ReadVideoFrame();
            }
        }

        static void ReEncoderTests()
        {
            using (var file = new VideoReEncoder())
            {
            var frameNumber = 0;

            file.StartReEncoding(_inputFile, 
                _outputFile,
                (Bitmap image, ref bool cancel) =>
                {
                    frameNumber++;

                    using (var g = Graphics.FromImage(image))
                    {
                        g.SmoothingMode = SmoothingMode.AntiAlias;
                        g.InterpolationMode = InterpolationMode.HighQualityBicubic;
                        g.PixelOffsetMode = PixelOffsetMode.HighQuality;
                        g.DrawString("Frame number " + frameNumber, new Font("Tahoma", 24), Brushes.Black,
                            new PointF(0, 0));

                        g.Flush();
                        g.Dispose();
                    }

                    //var destination = Path.Combine(AppDomain.CurrentDomain.BaseDirectory,
                    //    "test" + frameNumber + ".bmp");
                    //if(File.Exists(destination))
                    //    File.Delete(destination);

                    //image.Save(destination);

                    return image;
                });
            }
        }
    }
}
