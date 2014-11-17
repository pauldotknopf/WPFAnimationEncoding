using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.IO;
using System.IO.Pipes;
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Animation;
using System.Windows.Media.Imaging;
using Sandbox.WPF;
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

            Logging.Init();

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
            fileWriter.Open(_outputFile, fileReader.Width, fileReader.Height, fileReader.FrameRate, VideoCodec.Default, 30000000);

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
            AnimationAlongAPathExample animation = null;
            
            using (var file = new VideoReEncoder())
            {
                var frameNumber = 0;

                file.StartReEncoding(_inputFile,
                    _outputFile,
                    (Bitmap image, ref bool cancel) =>
                    {
                        frameNumber++;
                        Bitmap animationBitmap = null;

                        PrintingXamlHelper.Invoke(() =>
                        {
                            if (animation == null)
                            {
                                animation = new AnimationAlongAPathExample();
                                var size = new System.Windows.Size(image.Width, image.Height);
                                animation.Measure(size);
                                animation.Arrange(new Rect(size));
                                animation.UpdateLayout();
                                animation.MainStoryboard.Pause();
                                animation.MainStoryboard.Begin();
                            }

                            animation.MainStoryboard.SeekAlignedToLastTick(TimeSpan.FromMilliseconds(frameNumber * 10));

                            const double dpi = 96d;
                            var render = new RenderTargetBitmap((int)(image.Width / 96d * dpi), (int)(image.Height / 96d * dpi), dpi, dpi, PixelFormats.Pbgra32);

                            render.Render(animation);

                            var stream = new MemoryStream();
                            BitmapEncoder encoder = new PngBitmapEncoder();
                            encoder.Frames.Add(BitmapFrame.Create(render));
                            encoder.Save(stream);

                            animationBitmap = new Bitmap(stream);
                        });

                        if(animationBitmap == null)
                            throw new Exception("Couldn't generate an animation");
                        using (var g = Graphics.FromImage(image))
                        {
                            g.SmoothingMode = SmoothingMode.AntiAlias;
                            g.InterpolationMode = InterpolationMode.HighQualityBicubic;
                            g.PixelOffsetMode = PixelOffsetMode.HighQuality;
                            g.DrawImage(animationBitmap, new Rectangle(0,0, animationBitmap.Width, animationBitmap.Height));
                            g.Flush();
                            g.Dispose();
                        }
                        return image;
                    });
            }
        }
    }
}
