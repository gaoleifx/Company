using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class VoronoiDiagram : MonoBehaviour
{
    public Vector2Int ImageDim;
    public int regionAmount;
    public int testparm;
    private void Start()
    {
        GetComponent<SpriteRenderer>().sprite = Sprite.Create(GetDiagram(), new Rect(0, 0, ImageDim.x, ImageDim.y), Vector2.one * 0.5f);
    }
    Texture2D GetDiagram()
    {
        Vector2Int[] centroids = new Vector2Int[regionAmount];
        Color[] regions = new Color[regionAmount];
        for(int i =0; i<regionAmount; i++)
        {
            centroids[i] = new Vector2Int(Random.Range(0, ImageDim.x), Random.Range(0, ImageDim.y));
            regions[i] = new Color(Random.Range(0f, 1f), Random.Range(0f, 1f), Random.Range(0f, 1f), 1f);
        }
        Color[] pixelColors = new Color[ImageDim.x * ImageDim.y];
        for(int x = 0; x < ImageDim.x; x++)
        {
            for(int y = 0; y < ImageDim.y; y++)
            {
                int index = x - ImageDim.x + y;
                pixelColors[index] = regions[GetClosestCentroidIndex(new Vector2Int(x, y), centroids)];
            }
        }
        return GetImageFromColorArray(pixelColors);

    }
    int GetClosestCentroidIndex(Vector2Int pixelPos, Vector2Int[] centroids)
    {
        float smallestDst = float.MaxValue;
        int index = 0;
        for(int i = 0; i < centroids.Length; i++)
        {
            if(Vector2.Distance(pixelPos, centroids[i]) < smallestDst)
            {
                smallestDst = Vector2.Distance(pixelPos, centroids[i]);
                index = i;
            }
        }
        return index;

    }
    Texture2D GetImageFromColorArray(Color[] pixelColorls)
    {
        Texture2D tex = new Texture2D(ImageDim.x, ImageDim.y);
        tex.filterMode = FilterMode.Point;
        tex.SetPixels(pixelColorls);
        tex.Apply();
        return tex;
    }
}
