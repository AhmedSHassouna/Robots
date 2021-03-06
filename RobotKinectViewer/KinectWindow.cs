//------------------------------------------------------------------------------
// <copyright file="KinectWindow.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
//------------------------------------------------------------------------------

namespace RobotKinectViewer
{
    using System.Windows;
    using Microsoft.Kinect;
    using KinectWpfViewers;

    /// <summary>
    /// This is the core kinect window.
    /// </summary>
    public class KinectWindow : Window
    {
        #region Private state
        private readonly KinectDiagnosticViewer kinectDiagnosticViewer;
        private KinectSensor kinect;
        #endregion Private state

        public KinectWindow()
        {
            this.kinectDiagnosticViewer = new KinectDiagnosticViewer();
            this.kinectDiagnosticViewer.KinectColorViewer.CollectFrameRate = true;
            this.kinectDiagnosticViewer.KinectDepthViewer.CollectFrameRate = true;
            Content = this.kinectDiagnosticViewer;
            Width = 680;
            Height = 400;
            // DAVES> Position the Kinect Viewer at the Bottom Left of the display
            Left = SystemParameters.PrimaryScreenWidth - (double)Width - 1;
            Top = SystemParameters.PrimaryScreenHeight - (double)Height - 40;
            Title = "Robot Kinect Viewer";
            this.Closed += this.KinectWindowClosed;
        }

        public KinectSensor Kinect
        {
            get
            {
                return this.kinect;
            }

            set
            {
                this.kinect = value;
                this.kinectDiagnosticViewer.Kinect = this.kinect;
            }
        }

        public void StatusChanged()
        {
            this.kinectDiagnosticViewer.StatusChanged();
        }

        private void KinectWindowClosed(object sender, System.EventArgs e)
        {
            // KinectDiagnosticViewer will call kinectSensor.Stop() so we properly release its use.
            this.Kinect = null;
        }
    }
}
